#include "lockedkey.h"
#include <openssl/crypto.h>
#include <cstring>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
bool LockedKey::set(const QByteArray &key) {
    clear();
    if(key.size()!=32) return false;
    if(!m_page) {
#ifdef Q_OS_WIN
        SYSTEM_INFO info; GetSystemInfo(&info); m_size=info.dwPageSize;
        m_page=VirtualAlloc(nullptr,m_size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
        if(m_page && !VirtualLock(m_page,m_size)) { VirtualFree(m_page,0,MEM_RELEASE); m_page=nullptr; }
#else
        const long pageSize=sysconf(_SC_PAGESIZE);
        if(pageSize<=0) return false;
        m_size=size_t(pageSize);
        m_page=mmap(nullptr,m_size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
        if(m_page==MAP_FAILED) m_page=nullptr;
        if(m_page && mlock(m_page,m_size)!=0) { munmap(m_page,m_size); m_page=nullptr; }
#ifdef MADV_DONTDUMP
        if(m_page && madvise(m_page,m_size,MADV_DONTDUMP)!=0) { munlock(m_page,m_size); munmap(m_page,m_size); m_page=nullptr; }
#endif
#endif
    }
    if(!m_page) return false;
    std::memcpy(m_page,key.constData(),32); m_present=true; return true;
}
QByteArray LockedKey::copy() const { return m_present ? QByteArray(static_cast<const char *>(m_page),32) : QByteArray(); }
void LockedKey::clear() { if(m_page) OPENSSL_cleanse(m_page,m_size); m_present=false; }
LockedKey::~LockedKey() {
    clear();
    if(!m_page) return;
#ifdef Q_OS_WIN
    VirtualUnlock(m_page,m_size); VirtualFree(m_page,0,MEM_RELEASE);
#else
    munlock(m_page,m_size); munmap(m_page,m_size);
#endif
}
