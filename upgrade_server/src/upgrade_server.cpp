#include "upgrade_server.h"
// #include "config.h"
#include <stdlib.h>
#include <iostream>
#include <string>
#include <bitset>
#include <fstream>
#include <sstream>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>

extern "C"{
    extern uint32_t seedToKey(uint32_t seed, uint32_t Learmask);
}
UpgradeServer::UpgradeServer(){
    REGISTER_EV_HANDLER(UDS_SRV_EVT_SecAccessRequestSeed, HandleRequestSeed);
    REGISTER_EV_HANDLER(UDS_SRV_EVT_SecAccessValidateKey, HandleValidateKey);
    REGISTER_EV_HANDLER(UDS_SRV_EVT_DiagSessCtrl, HandleDiagSessCtrl);
    REGISTER_EV_HANDLER(UDS_SRV_EVT_EcuReset, HandleEcuReset);
    REGISTER_EV_HANDLER(UDS_SRV_EVT_ReadDataByIdent, HandleReadDataByIdent);
    REGISTER_EV_HANDLER(UDS_SRV_EVT_ReadMemByAddr, HandleReadMemByAddr);
    REGISTER_EV_HANDLER(UDS_SRV_EVT_CommCtrl, HandleCommCtrl);
    REGISTER_EV_HANDLER(UDS_SRV_EVT_WriteDataByIdent, HandleWriteDataByIdent);
    REGISTER_EV_HANDLER(UDS_SRV_EVT_RoutineCtrl, HandleRoutineCtrl);
    REGISTER_EV_HANDLER(UDS_SRV_EVT_RequestDownload, HandleRequestDownload);
    REGISTER_EV_HANDLER(UDS_SRV_EVT_RequestUpload, HandleRequestUpload);
    REGISTER_EV_HANDLER(UDS_SRV_EVT_TransferData, HandleTransferData);
    REGISTER_EV_HANDLER(UDS_SRV_EVT_RequestTransferExit, HandleRequestTransferExit);
    REGISTER_EV_HANDLER(UDS_SRV_EVT_RequestFileTransfer, HandleRequestFileTransfer);
    REGISTER_EV_HANDLER(UDS_SRV_EVT_SessionTimeout, HandleSessionTimeout);
    REGISTER_EV_HANDLER(UDS_SRV_EVT_DoScheduledReset, HandleDoScheduledReset);
}
uint8_t UpgradeServer::ProcessUdsEvent(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg){
    if (processors_.count(ev) == 0)return kServiceNotSupported;
    return processors_[ev](srv, arg);
}

uint8_t UpgradeServer::HandleRequestSeed(UDSServer_t *srv, const void *arg){
    UDSSecAccessRequestSeedArgs_t *req = (UDSSecAccessRequestSeedArgs_t *)arg;
    // use urandom to generate a random seed
    FILE *f = fopen("/dev/urandom", "r");
    if (!f) {
        fprintf(stderr, "Failed to open /dev/urandom\n");
        return kGeneralReject;
    }
    uint8_t buf[4];
    fread(&buf, sizeof(buf), 1, f);
    fclose(f);
    seed_ = (((uint32_t)buf[0]) << 24) + (((uint32_t)buf[1]) << 16)
            + (((uint32_t)buf[2]) << 8) + (uint32_t)buf[3];
    return req->copySeed(srv, &buf, sizeof(buf));
}

uint8_t UpgradeServer::HandleValidateKey(UDSServer_t *srv, const void *arg){
    UNUSED(srv);
    UDSSecAccessValidateKeyArgs_t *req = (UDSSecAccessValidateKeyArgs_t *)arg;
    // bool valid = false;
    auto key = seedToKey(seed_, 0x7A6B5C4D);
    if (req->len != sizeof(key)){
        printf("key len incorrected\n");
        return kGeneralReject;
    }
    printf("seed value %x key vaule %x\n", seed_, key);
    printf("req key vaule %02x%02x%02x%02x\n", req->key[0], req->key[1], req->key[2], req->key[3]);
    uint32_t req_key = (((uint32_t)req->key[0]) << 24) + (((uint32_t)req->key[1]) << 16)
                        + (((uint32_t)req->key[2]) << 8) + (uint32_t)req->key[3];
    printf("vaule %x\n", req_key);
    if (req_key != key) {
        printf("key verify failed\n");
        return kGeneralReject;
    } else {
        printf("Security level %d unlocked\n", req->level);
    }
    return kPositiveResponse;
}

uint8_t UpgradeServer::HandleDiagSessCtrl(UDSServer_t *srv, const void *arg){
    UDSDiagSessCtrlArgs_t *req = (UDSDiagSessCtrlArgs_t *)arg;
    req->p2_ms = srv->p2_ms;
    req->p2_star_ms = srv->p2_star_ms;
    return kPositiveResponse;
}
uint8_t UpgradeServer::HandleEcuReset(UDSServer_t *srv, const void *arg){
    UNUSED(srv);
    UDSECUResetArgs_t *req = (UDSECUResetArgs_t *)arg;
    req->powerDownTimeMillis = 500;
    return kPositiveResponse;
}
uint8_t UpgradeServer::HandleReadDataByIdent(UDSServer_t *srv, const void *arg){
    UDSRDBIArgs_t *req = (UDSRDBIArgs_t *)arg;
    switch (req->dataId)
    {
    case 0xF195:{
        FILE *f = fopen("/etc/bsp_version", "r");
        unsigned char buf[21] =  {0xff};
        if (f){
            fread(buf, sizeof(buf), 1, f);
            fclose(f);
        }
        req->copy(srv, buf, sizeof(buf));
        /* code */
        break;
    }
    case 0xF15A:{
        std::string did_name = GetDidPath() + "/F15A";
        FILE *f = fopen(did_name.c_str(), "rb");
        unsigned char buf[12] =  {0xff};
        if (f){
            fread(buf, sizeof(buf), 1, f);
            fclose(f);
        }
        req->copy(srv, buf, sizeof(buf));
       /* code */
        break;
    }
    
    default:
        return kRequestOutOfRange;
        break;
    }
    return kPositiveResponse;
}
uint8_t UpgradeServer::HandleReadMemByAddr(UDSServer_t *srv, const void *arg){
    UNUSED(srv);
    UNUSED(arg);
    return kServiceNotSupported;
}
uint8_t UpgradeServer::HandleCommCtrl(UDSServer_t *srv, const void *arg){
    UNUSED(srv);
    UNUSED(arg);
    return kServiceNotSupported;
}
uint8_t UpgradeServer::HandleWriteDataByIdent(UDSServer_t *srv, const void *arg){
    UNUSED(srv);
    UDSWDBIArgs_t *req = (UDSWDBIArgs_t *)arg;
    switch (req->dataId)
    {
    case 0xF15A:{
        struct stat info;
        if (stat(GetDidPath().c_str(), &info) != 0) {
            if (mkdir(GetDidPath().c_str(), S_IRWXU) != 0) {
                printf("create folder failed %d\n", errno);
                return kFailurePreventsExecutionOfRequestedAction;
            }
        }
        std::string did_name = GetDidPath() + "/F15A";
        FILE *f = fopen(did_name.c_str(), "wb+");
        if (f == nullptr){
            printf("create file failed %d\n", errno);
            return kFailurePreventsExecutionOfRequestedAction;
        }
        size_t size = fwrite(req->data, 1, req->len, f);
        fclose(f);
        if (size != req->len){
            remove(did_name.c_str());
            printf("write file failed %d\n", errno);
            return kFailurePreventsExecutionOfRequestedAction;
        }
        break;
    }
    
    default:
        return kRequestOutOfRange;
        break;
    }
    return kPositiveResponse;

}
static int calcFileChecksum(std::string &file, uint8_t *checksum){
    char buffer[128];
    std::string cmd = "md5sum " + file;
    std::string result = "";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    try {
        while (fgets(buffer, sizeof buffer, pipe) != NULL) {
            result += buffer;
        }
    } catch (...) {
        pclose(pipe);
        throw;
    }
    pclose(pipe);
    printf("got md5 str - %s\n", result.c_str());
    result.erase(result.find_first_of(' '));
    printf("got md5 str - %s\n", result.c_str());
    for (size_t i = 0; i < result.length(); i += 2) { 
        // Extract two characters representing a byte 
        std::string byteString = result.substr(i, 2); 
  
        // Convert the byte string to a uint8_t value 
        uint8_t byteValue = static_cast<uint8_t>( 
            stoi(byteString, nullptr, 16)); 
  
        // Add the byte to the byte array 
        checksum[i/2] = byteValue; 
    } 
//     return result;
// }
    return 0;
}
uint8_t UpgradeServer::HandleRoutineCtrl(UDSServer_t *srv, const void *arg){
    UDSRoutineCtrlArgs_t *req = (UDSRoutineCtrlArgs_t *)arg;
    switch (req->id)
    {
    case 0x0202:{
        /*checksum*/
        if(req->ctrlType != kStartRoutine){
            return kSubFunctionNotSupported;
        }
        if(req->len != 16){
            return kIncorrectMessageLengthOrInvalidFormat;
        }
        uint8_t chsum[16];
        if(calcFileChecksum(fw_filename_, chsum)){
            return kFailurePreventsExecutionOfRequestedAction;
        }
        uint8_t result = 0;
        for (size_t i = 0; i < sizeof chsum; i++)  {
            if(chsum[i] != req->optionRecord[i]){
                result = 1;
                break;
            }
        }
        
        req->copyStatusRecord(srv, &result, sizeof(result));
        break;
    }
    case 0x0203:{
        /* check program dependency*/
        if(req->ctrlType != kStartRoutine){
            return kSubFunctionNotSupported;
        }
        uint8_t result = 0;
        req->copyStatusRecord(srv, &result, sizeof(result));
        break;
    }
    // case 0xFF00:{
    //     /* erase */
    //     return kSubFunctionNotSupported;
    //     break;
    // }
    case 0xFF02:{
        if(req->ctrlType == kStartRoutine){
            /* start upgrade fw */
            auto result = RequestUpgradeFw();
            req->copyStatusRecord(srv, &result, sizeof(result));
        }else if(req->ctrlType == kRequestRoutineResults){
            /* query upgrade progress */
            auto result = QueryUpgradeProgress();
            req->copyStatusRecord(srv, &result, sizeof(result));
        }else{
            return kSubFunctionNotSupported;
        }
        break;
    }
    
    default:
        return kSubFunctionNotSupported;
        break;
    }
    return kPositiveResponse;

}
uint8_t UpgradeServer::HandleRequestDownload(UDSServer_t *srv, const void *arg){
    UNUSED(srv);
    UNUSED(arg);
    return kServiceNotSupported;
}
uint8_t UpgradeServer::HandleRequestUpload(UDSServer_t *srv, const void *arg){
    UNUSED(srv);
    UNUSED(arg);
    return kServiceNotSupported;
}
uint8_t UpgradeServer::HandleTransferData(UDSServer_t *srv, const void *arg){
    UNUSED(srv);
    UDSTransferDataArgs_t *req = (UDSTransferDataArgs_t *)arg;

    if (fw_file_ == nullptr){
        printf("file is not opened\n");
        return kGeneralProgrammingFailure;
    }
    size_t size = fwrite(req->data, 1, req->len, fw_file_);
    if (size != req->len){
        printf("Write file failed with size %ld %d\n", size, req->len);
        return kGeneralProgrammingFailure;
    }
    return kPositiveResponse;
}
uint8_t UpgradeServer::HandleRequestTransferExit(UDSServer_t *srv, const void *arg){
    UNUSED(srv);
    UNUSED(arg);
    // UDSRequestTransferExitArgs_t *req = (UDSRequestTransferExitArgs_t *)arg;
    // req->copyResponse()
    fclose(fw_file_);
    fw_file_ = nullptr;
    system("sync");
    return kPositiveResponse;
}
uint8_t UpgradeServer::HandleRequestFileTransfer(UDSServer_t *srv, const void *arg){
    UNUSED(srv);
    printf("HandleRequestFileTransfer entry\n");
    UDSRequestFileTransferArgs_t *req = (UDSRequestFileTransferArgs_t *)arg;
    std::string filename((const char*)req->filePath, req->filePathLen);
    auto pos = filename.find_last_of("/");
    if(pos != std::string::npos){
        filename.erase(0,pos);
    }
    fw_filename_ = GetDataPath() + "/" + filename;

    fw_file_ = fopen(fw_filename_.c_str(), "wb+");
    if (fw_file_ == nullptr){
        printf("fail to create file %s\n", fw_filename_.c_str());
        return kUploadDownloadNotAccepted;
    }
    req->maxNumberOfBlockLength = UDS_TP_MTU;
    printf("block size %d\n", req->maxNumberOfBlockLength);
    return kPositiveResponse;
}
uint8_t UpgradeServer::HandleSessionTimeout(UDSServer_t *srv, const void *arg){
    UNUSED(srv);
    UNUSED(arg);
    return kPositiveResponse;
}
uint8_t UpgradeServer::HandleDoScheduledReset(UDSServer_t *srv, const void *arg){
    UNUSED(srv);
    UNUSED(arg);
    std::cout << "reboot the system!!\n";
    srv->ecuResetScheduled = 0;
    RebootDevice();
    return kPositiveResponse;
}

UpgradeServerT5::UpgradeServerT5(): UpgradeServer(){
    did_path_ = "/data/dids";
    data_path_ = "/data";
}
uint8_t UpgradeServerT5::RequestUpgradeFw(){
    upgrade_thread_ = std::thread([this](){
        mkdir("/data/ota_package",S_IRWXU);
        std::string cmd = "tar xvf " + GetFwFileName() + " -C /data/ota_package";
        system(cmd.c_str());
        std::ifstream prop("/data/ota_package/payload_properties.txt");
        std::stringstream str_stream;
        str_stream << prop.rdbuf();
        cmd = "update_engine_client --update --payload=file:///data/ota_package/payload.bin --headers=\"" + str_stream.str() + "\"";
        system(cmd.c_str());
    });
    return upgrade_thread_.joinable() ? 0 : 1;
}
uint8_t UpgradeServerT5::QueryUpgradeProgress(){
    if(upgrade_thread_.joinable())return 2;
    std::string cmd = "update_engine_client --check";
    system(cmd.c_str());
    return 0;
}
void UpgradeServerT5::RebootDevice(){
    system("reboot");
}
UpgradeServerTest::UpgradeServerTest() : UpgradeServer(){
    did_path_ = "/tmp/dids";
    data_path_ = "/tmp";
}
uint8_t UpgradeServerTest::RequestUpgradeFw(){
    upgrading_ = true;
    upgrade_thread_ = std::thread([this](){
        std::this_thread::sleep_for(std::chrono::minutes(5));
        upgrading_ = false;
    });
    return upgrade_thread_.joinable() ? 0 : 1;
}
uint8_t UpgradeServerTest::QueryUpgradeProgress(){
    if(upgrading_)return 2;
    return 0;
}
