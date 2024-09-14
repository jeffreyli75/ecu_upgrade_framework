# ecu_upgrade_framework
在智驾汽车、无人车等系统中，多SOC、多ECU被用来协同处理传感器信号、路径规划等任务。不同的处理器连接在车载以太网、CAN总线、LIN总线以及其他各种网络上，组成一个复杂的网络系统。而随着技术迭代加快，OTA升级已经成为这些系统中不可或缺的功能，本项目尝试探索在UDS框架下建立多SOC、ECU环境下统一的升级框架，方便实现OTA升级服务。
## upgrade_server
SOC用于实现UDS服务、接收升级包的程序实例。
程序使用开源库[iso14229](https://github.com/driftregion/iso14229)配合[libdoip](https://github.com/GerritRiesch94/libdoip)实现UDS在DoIP上承载。
## upgrade_client
Client端设计运行于诊断仪上或负责OTA升级的SOC上，采用Python语言实现，采用udsoncan和doipclient库实现UDS升级流程控制。
