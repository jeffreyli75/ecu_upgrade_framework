#ifndef UPGRADE_SERVER_H_
#define UPGRADE_SERVER_H_

#include "iso14229.h"
#include <functional>
#include <unordered_map>
#include <string>
#include <thread>

#define UNUSED(x) (void)(x)
#define REGISTER_EV_HANDLER(ev, handler) \
            processors_.emplace(ev, [this](UDSServer_t *srv, const void *arg){\
                return handler(srv, arg); \
            })
class UpgradeServer {

public:
    using UdsEventProcessor = std::function<uint8_t(UDSServer_t*, const void *)>;
    UpgradeServer();
    uint8_t ProcessUdsEvent(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg);
    const std::string &GetFwFileName(){return fw_filename_;}
    const std::string &GetDidPath(){return did_path_;}
    const std::string &GetDataPath(){return data_path_;}
protected:
    std::string did_path_;
    std::string data_path_;
private:
    std::unordered_map<UDSServerEvent_t, UdsEventProcessor> processors_;
    uint32_t seed_ = {0};
    std::string fw_filename_;
    FILE *fw_file_ = nullptr;

    uint8_t HandleRequestSeed(UDSServer_t *srv, const void *arg);
    uint8_t HandleValidateKey(UDSServer_t *srv, const void *arg);
    uint8_t HandleDiagSessCtrl(UDSServer_t *srv, const void *arg);        
    uint8_t HandleEcuReset(UDSServer_t *srv, const void *arg);            
    uint8_t HandleReadDataByIdent(UDSServer_t *srv, const void *arg);     
    uint8_t HandleReadMemByAddr(UDSServer_t *srv, const void *arg);       
    uint8_t HandleCommCtrl(UDSServer_t *srv, const void *arg);            
    uint8_t HandleWriteDataByIdent(UDSServer_t *srv, const void *arg);    
    uint8_t HandleRoutineCtrl(UDSServer_t *srv, const void *arg);         
    uint8_t HandleRequestDownload(UDSServer_t *srv, const void *arg);     
    uint8_t HandleRequestUpload(UDSServer_t *srv, const void *arg);       
    uint8_t HandleTransferData(UDSServer_t *srv, const void *arg);        
    uint8_t HandleRequestTransferExit(UDSServer_t *srv, const void *arg); 
    uint8_t HandleRequestFileTransfer(UDSServer_t *srv, const void *arg); 
    uint8_t HandleSessionTimeout(UDSServer_t *srv, const void *arg);      
    uint8_t HandleDoScheduledReset(UDSServer_t *srv, const void *arg);

    virtual uint8_t RequestUpgradeFw() =  0;
    virtual uint8_t QueryUpgradeProgress() = 0;
    virtual void RebootDevice() =  0;

};

class UpgradeServerT5 :  public UpgradeServer {
public:
    UpgradeServerT5();
private:
    std::thread upgrade_thread_;
    virtual uint8_t RequestUpgradeFw();
    virtual uint8_t QueryUpgradeProgress();
    virtual void RebootDevice();
};
class UpgradeServer8155 :  public UpgradeServer {
    virtual uint8_t RequestUpgradeFw();
    virtual uint8_t QueryUpgradeProgress();
    virtual void RebootDevice();
};
class UpgradeServerTest :  public UpgradeServer {
public:
    UpgradeServerTest();
private:
    std::thread upgrade_thread_;
    bool upgrading_ = false;
    virtual uint8_t RequestUpgradeFw();
    virtual uint8_t QueryUpgradeProgress();
    virtual void RebootDevice(){}
};

#endif /* UPGRADE_SERVER_H_ */