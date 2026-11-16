#pragma once
#include <cstdlib>
#include <new>
#include <cstddef>
// Only linked into test executables. Emulates an x86 heap that has enough
// aggregate space but cannot satisfy a large contiguous allocation.
static std::size_t allocationCeiling=~std::size_t(0),largestAllocation=0,rejectedAllocation=0;
void* operator new(std::size_t size){
 if(size>allocationCeiling){rejectedAllocation=size;throw std::bad_alloc();}
 if(size>largestAllocation)largestAllocation=size;
 if(void* p=std::malloc(size?size:1))return p;throw std::bad_alloc();
}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
struct AllocationCeiling {
 std::size_t previous;
 explicit AllocationCeiling(std::size_t limit):previous(allocationCeiling){allocationCeiling=limit;largestAllocation=rejectedAllocation=0;}
 ~AllocationCeiling(){allocationCeiling=previous;}
};
