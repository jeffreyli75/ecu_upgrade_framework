#ifndef DOIPTP_H_
#define DOIPTP_H_

#include "DoIPServer.h"
#include "iso14229.h"
#include <mutex>

class DoIPTp : public UDSTpHandle{

public:
    DoIPTp() = default;
    int InitServer(uint16_t logical_addr, uint64_t gid, uint64_t eid);
    ssize_t DoipTpPeek(uint8_t **p_buf, UDSSDU_t *info);
    void DoipTpAckRecv();
    ssize_t DoipTpSend(uint8_t *buf, size_t len, UDSSDU_t *info);
    ssize_t DoipTpGetSendBuf(uint8_t **p_buf);

private:
    // UDSTpHandle_t tp_hdl_;
    DoIPServer doip_server_;
    unsigned short logical_server_addr_;
    std::unique_ptr<DoIPConnection> connection_;
    std::vector<std::thread> doip_receiver_;
    bool server_active_ = false;
    uint8_t recv_buf_[UDS_TP_MTU];
    uint8_t send_buf_[UDS_TP_MTU];
    size_t recv_len_;
    size_t acked_len_;
    std::mutex buf_guard_;


    void ListenUdp();
    void ListenTcp();
    void ReceiveFromLibrary(unsigned short address, unsigned char* data, int length);
    bool DiagnosticMessageReceived(unsigned short targetAddress);
    void CloseConnection();


};

#endif /* DOIPTP_H_ */