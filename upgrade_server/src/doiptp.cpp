#include "doiptp.h"

#include<iostream>
#include<iomanip>
#include<thread>

static UDSTpStatus_t DoipTpPollWrap(UDSTpHandle_t *hdl) { return 0; }

static ssize_t DoipTpPeekWrap(UDSTpHandle_t *hdl, uint8_t **p_buf, UDSSDU_t *info) {
    assert(hdl);
    assert(p_buf);
    DoIPTp *impl = static_cast<DoIPTp *>(hdl);
    return impl->DoipTpPeek(p_buf, info);

//     *p_buf = impl->recv_buf;
//     if (impl->recv_len) { // recv not yet acked
//         ret = impl->recv_len;
//         goto done;
//     }

//     UDSSDU_t *msg = &impl->recv_info;

//     // recv acked, OK to receive
//     ret = tp_recv_once(impl->phys_fd, impl->recv_buf, sizeof(impl->recv_buf));
//     if (ret > 0) {
//         msg->A_TA = impl->phys_sa;
//         msg->A_SA = impl->phys_ta;
//         msg->A_TA_Type = UDS_A_TA_TYPE_PHYSICAL;
//     } else {
//         ret = tp_recv_once(impl->func_fd, impl->recv_buf, sizeof(impl->recv_buf));
//         if (ret > 0) {
//             msg->A_TA = impl->func_sa;
//             msg->A_SA = impl->func_ta;
//             msg->A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL;
//         }
//     }

//     if (ret > 0) {
//         fprintf(stdout, "%06d, %s recv, 0x%03x (%s), ", UDSMillis(), impl->tag, msg->A_TA,
//                 msg->A_TA_Type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
//         for (unsigned i = 0; i < ret; i++) {
//             fprintf(stdout, "%02x ", impl->recv_buf[i]);
//         }
//         fprintf(stdout, "\n");
//         fflush(stdout); // flush every time in case of crash
//         // UDS_DBG_PRINT("<<< ");
//         // UDS_DBG_PRINTHEX(, ret);
//     }

// done:
//     if (ret > 0) {
//         impl->recv_len = ret;
//         if (info) {
//             *info = *msg;
//         }
//     }
//     return ret;
}

static void DoipTpAckRecvWrap(UDSTpHandle_t *hdl) {
    assert(hdl);
    DoIPTp *impl = static_cast<DoIPTp *>(hdl);
    impl->DoipTpAckRecv();
}

static ssize_t DoipTpSendWrap(UDSTpHandle_t *hdl, uint8_t *buf, size_t len, UDSSDU_t *info) {
    assert(hdl);
    DoIPTp *impl = static_cast<DoIPTp *>(hdl);
    return impl->DoipTpSend(buf, len, info);
}

static ssize_t DoipTpGetSendBufWrap(UDSTpHandle_t *hdl, uint8_t **p_buf) {
    assert(hdl);
    DoIPTp *impl = static_cast<DoIPTp *>(hdl);
    return impl->DoipTpGetSendBuf(p_buf);
}


int DoIPTp::InitServer(uint16_t logical_addr, uint64_t gid, uint64_t eid) {
    // memset(&tp_hdl_, 0, sizeof(tp_hdl_));
    peek = DoipTpPeekWrap;
    send = DoipTpSendWrap;
    poll = DoipTpPollWrap;
    ack_recv = DoipTpAckRecvWrap;
    get_send_buf = DoipTpGetSendBufWrap;

    // VIN needs to have a fixed length of 17 bytes.
    // Shorter VINs will be padded with '0'
    // doip_server_.setVIN("FOOBAR");
    doip_server_.setLogicalGatewayAddress(logical_addr);
    doip_server_.setGID(gid);
    doip_server_.setFAR(0);
    doip_server_.setEID(eid);

    doip_server_.setupUdpSocket();

    server_active_ = true;
    doip_receiver_.push_back(std::thread([this](){
        ListenUdp();
    }));
    doip_receiver_.push_back(std::thread([this](){
        ListenTcp();
    }));

    doip_server_.sendVehicleAnnouncement();



    // UDS_DBG_PRINT("%s initialized phys link rx 0x%03x tx 0x%03x func link rx 0x%03x tx 0x%03x\n",
    //               strlen(tp->tag) ? tp->tag : "server", source_addr, target_addr, source_addr_func,
    //               target_addr);
    return UDS_OK;
}

/*
 * Check permantly if udp message was received
 */
void DoIPTp::ListenUdp() {

    while(server_active_) {
        doip_server_.receiveUdpMessage();
    }
}

/*
 * Check permantly if tcp message was received
 */
void DoIPTp::ListenTcp() {

    doip_server_.setupTcpSocket();

    while(true) {
        connection_ = doip_server_.waitForTcpConnection();
        connection_->setCallback(
            [this](unsigned short address, unsigned char* data, int length){ReceiveFromLibrary(address, data, length);}, 
            [this](unsigned short targetAddress){return DiagnosticMessageReceived(targetAddress);}, 
            [this](){CloseConnection();});
        connection_->setGeneralInactivityTime(50000);

         while(connection_->isSocketActive()) {
             connection_->receiveTcpMessage();
         }
    }
}

/**
 * Is called when the doip library receives a diagnostic message.
 * @param address   logical address of the ecu
 * @param data      message which was received
 * @param length    length of the message
 */
void DoIPTp::ReceiveFromLibrary(unsigned short address, unsigned char* data, int length) {
    std::cout << "DoIP Message received from 0x" << std::hex << address << ": ";
    int debug_len = length > 32 ? 32 : length;
    for(int i = 0; i < debug_len; i++) {
        std::cout << std::hex << std::setw(2) << (int)data[i] << " ";
    }
    if(debug_len < length)std::cout << "...";
    std::cout << std::endl;

    std::unique_lock<std::mutex> lk(buf_guard_);
    memmove(recv_buf_, data, length);
    recv_len_ = length;
    
    // if(length > 2 && data[0] == 0x22)  {
    //     std::cout << "-> Send diagnostic message positive response" << std::endl;
    //     unsigned char responseData[] = { 0x62, data[1], data[2], 0x01, 0x02, 0x03, 0x04};
    //     connection_->sendDiagnosticPayload(address, responseData, sizeof(responseData));
    // } else {
    //     std::cout << "-> Send diagnostic message negative response" << std::endl;
    //     unsigned char responseData[] = { 0x7F, data[0], 0x11};
    //     connection_->sendDiagnosticPayload(address, responseData, sizeof(responseData));
    // }


}

/**
 * Will be called when the doip library receives a diagnostic message.
 * The library notifies the application about the message.
 * Checks if there is a ecu with the logical address
 * @param targetAddress     logical address to the ecu
 * @return                  If a positive or negative ACK should be send to the client
 */
bool DoIPTp::DiagnosticMessageReceived(unsigned short targetAddress) {
    (void)targetAddress;
    unsigned char ackCode;

    std::cout << "Received Diagnostic message" << std::endl;

    //send positiv ack
    ackCode = 0x00;
    std::cout << "-> Send positive diagnostic message ack" << std::endl;
    connection_->sendDiagnosticAck(logical_server_addr_, true, ackCode);

    return true;
}

/**
 * Closes the connection of the server by ending the listener threads
 */
void DoIPTp::CloseConnection() {
    std::cout << "Connection closed" << std::endl;
    //serverActive = false;
}

ssize_t DoIPTp::DoipTpPeek(uint8_t **p_buf, UDSSDU_t *info) {
    ssize_t ret = 0;
    std::unique_lock<std::mutex> lk(buf_guard_);
    *p_buf = recv_buf_;
    if(recv_len_ == acked_len_){
        ret = recv_len_;
    }else if(recv_len_ > acked_len_){
        ret = recv_len_;
        acked_len_ =  recv_len_;
    }
    // if((ret > 0) && (info != nullptr)){
    //     *info = *msg;
    // }

    return ret;
}

void DoIPTp::DoipTpAckRecv() {
    std::unique_lock<std::mutex> lk(buf_guard_);
    recv_len_ = 0;
    acked_len_ = 0;
}

ssize_t DoIPTp::DoipTpSend(uint8_t *buf, size_t len, UDSSDU_t *info) {
    connection_->sendDiagnosticPayload(logical_server_addr_, buf, len);
    return len;
}

ssize_t DoIPTp::DoipTpGetSendBuf(uint8_t **p_buf) {
    *p_buf = send_buf_;
    return sizeof(send_buf_);
}




