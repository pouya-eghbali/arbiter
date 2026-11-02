#pragma once
#include "../../src/biometricauth.h"
class TestDeviceAuth final : public BiometricAuth {
public:
    TestDeviceAuth() { refresh(); }
    bool foregroundRequired() const override { return false; }
    QString name() const override { return "Test device"; }
    void refresh() override { availability(true,{}); }
protected:
    void startNative(const QString &,quint64 token) override { complete(token,true); }
    void cancelNative() override {}
};
