#include <Base.h>
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MallocLib.h>

#define CPOOL_HEAD_SIGNATURE   SIGNATURE_32('C','p','h','d')

//
// DB410c notu: Onceki surum cache-line hizali alloc icin AllocateAlignedPages
// + FreeAlignedPages kullaniyordu; free yolunda FreePages "Invalid Parameter"
// verip her blok okumada ASSERT [MMCHS] spam'i uretiyordu. Bu surum TAMAMEN
// AllocatePool/FreePool tabanli: fazladan tampon alip data isaretcisini
// Boundary'ye hizaliyor, orijinal pool isaretcisini head'de saklayip FreePool
// ile guvenle birakiyor. (DMA icin cache-line hizalama korunur.)
//
typedef struct {
  UINT32  Signature;
  VOID   *AllocBase;   // AllocatePool'dan donen orijinal isaretci (FreePool icin)
  UINTN   Size;        // istenen kullanici boyutu (realloc icin)
} CPOOL_HEAD;

VOID *memalign(UINTN Boundary, UINTN Size)
{
  VOID        *Base;
  CPOOL_HEAD  *Head;
  UINTN        DataAddr;
  UINTN        Total;

  if (Size == 0) {
    DEBUG((DEBUG_ERROR, "ERROR memalign: Zero Size\n"));
    return NULL;
  }

  if (Boundary < 8) {
    Boundary = 8;
  }

  // Head'in data'dan hemen once sigmasi + Boundary hizalama paylari icin yer.
  Total = sizeof(CPOOL_HEAD) + Boundary + Size;
  Base  = AllocatePool(Total);
  if (Base == NULL) {
    DEBUG((DEBUG_ERROR, "ERROR memalign: alloc failed\n"));
    return NULL;
  }

  // Data'yi Boundary'ye hizala; head tam onunde.
  DataAddr = ALIGN_VALUE((UINTN)Base + sizeof(CPOOL_HEAD), Boundary);
  Head     = (CPOOL_HEAD *)(DataAddr - sizeof(CPOOL_HEAD));

  Head->Signature = CPOOL_HEAD_SIGNATURE;
  Head->AllocBase = Base;
  Head->Size      = Size;

  return (VOID *)DataAddr;
}

VOID *malloc(UINTN Size)
{
  return memalign(8, Size);
}

VOID *calloc(UINTN Count, UINTN Size)
{
  VOID *Ptr;

  Ptr = malloc (Count * Size);
  if (Ptr) {
    SetMem (Ptr, Count * Size, 0);
  }

  return Ptr;
}

VOID free(VOID *Ptr)
{
  CPOOL_HEAD *Head;

  if (Ptr != NULL) {
    Head = (CPOOL_HEAD *)((UINTN)Ptr - sizeof(CPOOL_HEAD));
    if (Head->Signature == CPOOL_HEAD_SIGNATURE) {
      FreePool (Head->AllocBase);
    } else {
      DEBUG((DEBUG_ERROR, "ERROR free(0x%p): bad signature 0x%08X\n",
             Ptr, Head->Signature));
    }
  }
}

VOID *realloc(VOID *Ptr, UINTN NewSize)
{
  VOID       *RetVal = NULL;
  CPOOL_HEAD *Head    = NULL;
  UINTN      OldSize = 0;
  UINTN      NumCpy;

  if (Ptr != NULL) {
    Head = (CPOOL_HEAD *)((UINTN)Ptr - sizeof(CPOOL_HEAD));
    if (Head->Signature != CPOOL_HEAD_SIGNATURE) {
      DEBUG((DEBUG_ERROR, "ERROR realloc(0x%p): bad signature 0x%08X\n",
             Ptr, Head->Signature));
      return NULL;
    }
    OldSize = Head->Size;
  }

  if (NewSize == OldSize) {
    RetVal = Ptr;
  } else if (NewSize > 0) {
    RetVal = malloc(NewSize);
    if (Ptr != NULL) {
      if (RetVal != NULL) {
        NumCpy = MIN(OldSize, NewSize);
        CopyMem (RetVal, Ptr, NumCpy);
        free (Ptr);
      }
    }
  } else {
    free(Ptr);
  }

  return RetVal;
}
