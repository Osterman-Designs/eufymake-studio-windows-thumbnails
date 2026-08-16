#pragma once

#include <unknwn.h>

class EmpfClassFactory : public IClassFactory {
public:
    explicit EmpfClassFactory(REFCLSID clsid);

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;
    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override;
    IFACEMETHODIMP LockServer(BOOL lock) override;

private:
    ~EmpfClassFactory();

    long m_ref;
    CLSID m_clsid;
};
