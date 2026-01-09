#include <stdint.h>
#include <carlos/ahci.h>
#include <carlos/pci.h>
#include <carlos/iomap.h>
#include <carlos/mmio.h>
#include <carlos/klog.h>
#include <carlos/pmm.h>

// AHCI HBA regs offsets
enum {
  AHCI_CAP   = 0x00,
  AHCI_GHC   = 0x04,
  AHCI_IS    = 0x08,
  AHCI_PI    = 0x0C,
  AHCI_VS    = 0x10,
  AHCI_CAP2  = 0x24,

  AHCI_PORTS = 0x100,
  AHCI_PORT_SZ = 0x80,

  // per-port offsets
  P_IS   = 0x10,
  P_IE   = 0x14,
  P_CMD  = 0x18,
  P_TFD  = 0x20,
  P_SIG  = 0x24,
  P_SSTS = 0x28,
  P_SCTL = 0x2C,
  P_SERR = 0x30,
  P_SACT = 0x34,
  P_CI   = 0x38,
};

typedef struct __attribute__((packed)) {
  // DW0
  uint8_t  cfl:5;
  uint8_t  a:1;
  uint8_t  w:1;
  uint8_t  p:1;

  uint8_t  r:1;
  uint8_t  b:1;
  uint8_t  c:1;
  uint8_t  rsv0:1;
  uint8_t  pmp:4;

  uint16_t prdtl;
  volatile uint32_t prdbc;
  uint32_t ctba;
  uint32_t ctbau;
  uint32_t rsv1[4];
} HbaCmdHdr;

typedef struct __attribute__((packed)) {
  uint32_t dba;
  uint32_t dbau;
  uint32_t rsv0;
  uint32_t dbc:22;
  uint32_t rsv1:9;
  uint32_t i:1;
} HbaPrdt;

typedef struct __attribute__((packed)) {
  uint8_t  cfis[64];
  uint8_t  acmd[16];
  uint8_t  rsv[48];
  HbaPrdt  prdt[1]; // we use 1 entry for now
} HbaCmdTbl;

typedef struct {
  void    *clb;    // command list (1 page)
  void    *fb;     // FIS receive (1 page)
  void    *ctba;   // command table (1 page)
  int      inited;
} AhciPortState;

static AhciPortState g_ports[32];

static uint64_t abar = 0;

static uint64_t read_bar_mmio32(uint8_t b, uint8_t d, uint8_t f, int bar_index){
  uint16_t off = (uint16_t)(0x10 + bar_index * 4);
  uint32_t bar = pci_read32(b,d,f,off);
  if (bar == 0) return 0;
  if (bar & 1) return 0;                 // IO BAR, not MMIO
  return (uint64_t)(bar & ~0xFULL);      // 16-byte aligned
}

static const char* ahci_sig_name(uint32_t sig){
  switch (sig) {
    case 0x00000101: return "SATA";
    case 0xEB140101: return "SATAPI";
    case 0xC33C0101: return "SEMB";
    case 0x96690101: return "PM";
    default: return "UNKNOWN";
  }
}

static void ahci_dump_ports(uint64_t hba){
  uint32_t pi = mmio_read32(hba + AHCI_PI);

  for (uint32_t p = 0; p < 32; p++){
    if (((pi >> p) & 1u) == 0) continue;

    uint64_t pr = hba + AHCI_PORTS + (uint64_t)p * AHCI_PORT_SZ;

    uint32_t ssts = mmio_read32(pr + P_SSTS);
    uint32_t sig  = mmio_read32(pr + P_SIG);
    uint32_t tfd  = mmio_read32(pr + P_TFD);
    uint32_t serr = mmio_read32(pr + P_SERR);

    uint32_t det = (ssts >> 0) & 0xF;
    uint32_t ipm = (ssts >> 8) & 0xF;

    kprintf("AHCI: port %u: SSTS(det=%u ipm=%u) SIG=0x%x TFD=0x%x SERR=0x%x\n",
            p, det, ipm, sig, tfd, serr);

    // A common “device present” check:
    // det==3 => device present + PHY comm
    if (det == 3 && ipm == 1) {
      // signatures (common):
      // 0x00000101 SATA
      // 0xEB140101 SATAPI
      // 0xC33C0101 SEMB
      // 0x96690101 PM
      kprintf("AHCI: port %u: device present (%s)\n", p, ahci_sig_name(sig));
    }
  }
}

static void ahci_port_stop(uint64_t pr){
  uint32_t cmd = mmio_read32(pr + P_CMD);

  // Clear ST (start) and FRE (FIS receive enable)
  cmd &= ~(1u<<0);   // ST
  cmd &= ~(1u<<4);   // FRE
  mmio_write32(pr + P_CMD, cmd);

  // Wait until FR and CR clear
  for (int i=0; i<1000000; i++){
    uint32_t c = mmio_read32(pr + P_CMD);
    if (((c >> 14) & 1u) == 0 && ((c >> 15) & 1u) == 0) break; // FR, CR
  }
}

static void ahci_port_start(uint64_t pr){
  uint32_t cmd = mmio_read32(pr + P_CMD);
  cmd |= (1u<<4);  // FRE
  cmd |= (1u<<0);  // ST
  mmio_write32(pr + P_CMD, cmd);
}

static int ahci_port_init(uint32_t port){
  if (!abar) return -1;
  if (port >= 32) return -2;

  uint64_t hba = (uint64_t)(uintptr_t)iomap(abar, 0x2000, 0);
  uint64_t pr  = hba + AHCI_PORTS + (uint64_t)port * AHCI_PORT_SZ;

  // Only init if device present
  uint32_t ssts = mmio_read32(pr + P_SSTS);
  uint32_t det = (ssts >> 0) & 0xF;
  uint32_t ipm = (ssts >> 8) & 0xF;
  if (!(det == 3 && ipm == 1)) return -3;

  AhciPortState *ps = &g_ports[port];
  if (ps->inited) return 0;

  // Stop port before programming
  ahci_port_stop(pr);

  // Allocate pages for command list and FIS and command table
  ps->clb  = pmm_alloc_page();
  ps->fb   = pmm_alloc_page();
  ps->ctba = pmm_alloc_page();
  if (!ps->clb || !ps->fb || !ps->ctba) return -4;

  // Zero them
  for (int i=0;i<4096;i++) ((uint8_t*)ps->clb)[i]=0;
  for (int i=0;i<4096;i++) ((uint8_t*)ps->fb)[i]=0;
  for (int i=0;i<4096;i++) ((uint8_t*)ps->ctba)[i]=0;

  // Program CLB/FB (physical = virtual for now)
  uint64_t clb_phys = (uint64_t)(uintptr_t)ps->clb;
  uint64_t fb_phys  = (uint64_t)(uintptr_t)ps->fb;

  mmio_write32(pr + 0x00, (uint32_t)(clb_phys & 0xFFFFFFFF)); // PxCLB
  mmio_write32(pr + 0x04, (uint32_t)(clb_phys >> 32));        // PxCLBU
  mmio_write32(pr + 0x08, (uint32_t)(fb_phys & 0xFFFFFFFF));  // PxFB
  mmio_write32(pr + 0x0C, (uint32_t)(fb_phys >> 32));         // PxFBU

  // Setup command header slot 0 to point to our command table
  HbaCmdHdr *cl = (HbaCmdHdr*)ps->clb;
  uint64_t ct_phys = (uint64_t)(uintptr_t)ps->ctba;

  cl[0].cfl   = 5;       // 5 dwords = 20 bytes CFIS
  cl[0].w     = 0;       // read
  cl[0].prdtl = 1;       // 1 PRDT entry
  cl[0].ctba  = (uint32_t)(ct_phys & 0xFFFFFFFF);
  cl[0].ctbau = (uint32_t)(ct_phys >> 32);

  // Clear errors
  mmio_write32(pr + P_SERR, 0xFFFFFFFF);
  mmio_write32(pr + P_IS,   0xFFFFFFFF);

  // Start port
  ahci_port_start(pr);

  ps->inited = 1;
  return 0;
}

int ahci_probe_bdf(uint8_t b, uint8_t d, uint8_t f){
  // Ensure it's AHCI
  uint32_t classr = pci_read32(b,d,f,0x08);
  uint8_t class_code = (uint8_t)(classr >> 24);
  uint8_t subclass   = (uint8_t)(classr >> 16);
  uint8_t prog_if    = (uint8_t)(classr >> 8);

  if (!(class_code == 0x01 && subclass == 0x06 && prog_if == 0x01)){
    kprintf("AHCI: %u:%u.%u is not AHCI (class=%u:%u pi=%u)\n",
            b,d,f,class_code,subclass,prog_if);
    return -1;
  }

  // BAR5 is standard ABAR for AHCI
  uint64_t bar5 = read_bar_mmio32(b,d,f,5);
  if (!bar5){
    kprintf("AHCI: BAR5 is 0\n");
    return -2;
  }

  abar = bar5;
  uint64_t hba = (uint64_t)(uintptr_t)iomap(abar, 0x2000, 0);

  uint32_t cap  = mmio_read32(hba + AHCI_CAP);
  uint32_t ghc  = mmio_read32(hba + AHCI_GHC);
  uint32_t vs   = mmio_read32(hba + AHCI_VS);
  uint32_t cap2 = mmio_read32(hba + AHCI_CAP2);
  uint32_t pi   = mmio_read32(hba + AHCI_PI);

  // Enable AHCI mode if not enabled (AE = bit31)
  if ((ghc & (1u<<31)) == 0){
    mmio_write32(hba + AHCI_GHC, ghc | (1u<<31));
    ghc = mmio_read32(hba + AHCI_GHC);
  }

  kprintf("AHCI: bdf=%u:%u.%u ABAR=%p\n", b,d,f, (void*)(uintptr_t)abar);
  kprintf("AHCI: CAP=0x%x CAP2=0x%x GHC=0x%x VS=0x%x PI=0x%x\n", cap, cap2, ghc, vs, pi);

  ahci_dump_ports(hba);
  return 0;
}

int ahci_probe(void){
  uint8_t bs=0, be=0;
  if (pci_get_bus_range(&bs, &be) != 0){
    kprintf("AHCI: PCI not initialized\n");
    return -1;
  }

  for (uint16_t bus = bs; bus <= be; bus++){
    for (uint8_t dev = 0; dev < 32; dev++){
      for (uint8_t fun = 0; fun < 8; fun++){
        uint16_t vendor = pci_read16((uint8_t)bus, dev, fun, 0x00);
        if (vendor == 0xFFFF) { if (fun==0) break; else continue; }

        uint32_t classr = pci_read32((uint8_t)bus, dev, fun, 0x08);
        uint8_t class_code = (uint8_t)(classr >> 24);
        uint8_t subclass   = (uint8_t)(classr >> 16);
        uint8_t prog_if    = (uint8_t)(classr >> 8);

        if (class_code == 0x01 && subclass == 0x06 && prog_if == 0x01){
          return ahci_probe_bdf((uint8_t)bus, dev, fun);
        }

        // stop if not multifunction
        if (fun == 0){
          uint8_t header_type = pci_read8((uint8_t)bus, dev, fun, 0x0E);
          if ((header_type & 0x80) == 0) break;
        }
      }
    }
  }

  kprintf("AHCI: no controller found\n");
  return -2;
}

int ahci_read(uint32_t port, uint64_t lba, uint32_t count, void *buf){
  if (!buf || count == 0) return -1;
  
  // Auto-probe controller on first use
  if (!abar) {
    int prc = ahci_probe();
    if (prc != 0 || !abar) return -2;
  }

  int rc = ahci_port_init(port);
  if (rc != 0) return rc;

  uint64_t hba = (uint64_t)(uintptr_t)iomap(abar, 0x2000, 0);
  uint64_t pr  = hba + AHCI_PORTS + (uint64_t)port * AHCI_PORT_SZ;

  AhciPortState *ps = &g_ports[port];
  HbaCmdHdr *cl = (HbaCmdHdr*)ps->clb;
  HbaCmdTbl *tbl = (HbaCmdTbl*)ps->ctba;

  // Wait while busy
  for (int i=0; i<1000000; i++){
    uint32_t tfd = mmio_read32(pr + P_TFD);
    if ((tfd & (1u<<7)) == 0 && (tfd & (1u<<3)) == 0) break; // BSY=7, DRQ=3
  }

  // Clear command table
  for (size_t i = 0; i < sizeof(HbaCmdTbl); i++) {
    ((uint8_t*)tbl)[i] = 0;
  }

  // Setup PRDT: one buffer chunk
  uint64_t buf_phys = (uint64_t)(uintptr_t)buf;
  tbl->prdt[0].dba  = (uint32_t)(buf_phys & 0xFFFFFFFF);
  tbl->prdt[0].dbau = (uint32_t)(buf_phys >> 32);
  tbl->prdt[0].dbc  = (count * 512) - 1;   // byte count minus 1
  tbl->prdt[0].i    = 0;

  // Build CFIS (Host to Device Register FIS)
  uint8_t *cfis = tbl->cfis;
  cfis[0] = 0x27;  // FIS type: Reg H2D
  cfis[1] = 1<<7;  // C=1 (command)
  cfis[2] = 0x25;  // ATA READ DMA EXT
  cfis[3] = 0;     // features low

  // LBA (48-bit)
  cfis[4] = (uint8_t)(lba & 0xFF);
  cfis[5] = (uint8_t)((lba >> 8) & 0xFF);
  cfis[6] = (uint8_t)((lba >> 16) & 0xFF);
  cfis[7] = 1<<6;  // device: LBA mode
  cfis[8] = (uint8_t)((lba >> 24) & 0xFF);
  cfis[9] = (uint8_t)((lba >> 32) & 0xFF);
  cfis[10]= (uint8_t)((lba >> 40) & 0xFF);
  cfis[11]= 0;     // features high

  // sector count (16-bit)
  cfis[12]= (uint8_t)(count & 0xFF);
  cfis[13]= (uint8_t)((count >> 8) & 0xFF);
  cfis[14]= 0;
  cfis[15]= 0;

  // Ensure header is read, PRDTL is 1, read op
  cl[0].w = 0;
  cl[0].prdtl = 1;

  // Clear interrupts + errors
  mmio_write32(pr + P_IS, 0xFFFFFFFF);
  mmio_write32(pr + P_SERR, 0xFFFFFFFF);

  // Issue command in slot 0 by setting PxCI bit0
  mmio_write32(pr + P_CI, 1u);

  // Poll for completion
  for (int i=0; i<5000000; i++){
    uint32_t ci = mmio_read32(pr + P_CI);
    if ((ci & 1u) == 0) break;
  }

  // Check errors
  uint32_t is = mmio_read32(pr + P_IS);
  uint32_t tfd = mmio_read32(pr + P_TFD);
  if (is & (1u<<30)) { // TFES
    kprintf("AHCI: read error TFES, IS=0x%x TFD=0x%x\n", is, tfd);
    return -10;
  }

  return 0;
}