#include "platform.hpp"
#include "../source.hpp"

void* MemAlloc_Alloc( int size ) {
  return Interfaces::m_pMemAlloc->Alloc( size );
}

void *MemAlloc_Realloc( void *pMemBlock, int size ) {
  return Interfaces::m_pMemAlloc->Realloc( pMemBlock, size );
}

void MemAlloc_Free( void* pMemBlock ) {
  Interfaces::m_pMemAlloc->Free( pMemBlock );
}
