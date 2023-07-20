# Study
这个项目是模仿别人写的，很感谢他提供的开源项目。源代码作者https://github.com/EEEugene/qtWeatherForecast
我这里只写了部分功能，删除了查询国外的天气部分，只能查询国内天气
# 编写历程
1.因为设置了窗口属性为无边框setWindowFlag(Qt::FramelessWindowHint);导致窗口不能移动和随意拉伸。解决办法：重写mousePressEvent() 和 mouseMoveEvent()
网上有许多方法可自己试试。
2.在设计ui界面时，我原本打算直接在MainWindow窗口下设计stylesheet，但是点进去添加了border—image样式的背景图后，无法在界面显示出来。后来就重新拉了个widget
把其它控件都放进来，然后再对此widget设置背景。

3.源作者的仓库里没有看到城市与城市编码对应的json文件，可能是忘记传了。你可去网上下载个或者使用我这里面的。
4.

