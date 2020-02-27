LilyGO T-Watch Demo
================

## Description
1. Playing music is performed in the main loop. Touch operation will cause the sound to freeze. The noise is due to the ESP8266Audio library itself, which cannot be resolved temporarily. Some operations may cause crashes, mainly when the music is played.
2. FlappyBird must use the 25, 26 port baseboard to use external buttons. Double-click the user button to exit
3. Bluetooth uses https://github.com/lewisxhe/simple-ble for testing. Devices not on the list are not connected.
4. Thermometer uses mlx90614 sensor for external use on 21, 22 function baseboard
5. You must know that this project is in the incomplete stage



## 描述
1. 播放音乐在主循环中进行,使用触摸操作会导致声音卡顿,有噪音是因为ESP8266Audio库本身原因,暂时无法解决,有些操作可能导致死机,主要集中在播放音乐完成时
2. FlappyBird 使用外部按键必须使用25,26端口的底板,双击用户按键退出
3. Bluetooth 使用https://github.com/lewisxhe/simple-ble 进行测试,对未在列表中的设备,不进行连接
4. Thermometer 使用 mlx90614 传感器 外接在21,22的功能底板上使用
5. 你必须要知道,这个项目处于未完成阶段


### 资料:
- [中国各省市区简称一览表](http://www.tcmap.com.cn/list/jiancheng_list.html)
- [中国统计数据服务](http://data.acmr.com.cn/member/city/city_md.asp#11)
