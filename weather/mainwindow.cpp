#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QByteArray>
#include <QMenu>
#include <QMessageBox>
#include <QtNetwork/QNetworkReply>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QFile>
#include <QUrl>
#include <QPainter>

#define INCREMENT 2     //温度每升高或降低1度，y轴坐标的增量
#define POINT_RADIUS 3  //曲线秒点的大小
#define TEXT_OFFSET_X 9    //温度文本相对于x点的偏移
#define TEXT_OFFSET_Y 7    //温度文本相对于y点的偏移

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowFlag(Qt::FramelessWindowHint);

    //构建右键菜单
    mExitMenu = new QMenu(this);
    mExitAct = new QAction();
    mExitAct->setText(tr("退出"));
    mExitAct->setIcon(QIcon(":/img/close.png"));
    mExitMenu->addAction(mExitAct);
    //用lamba函数连接退出按钮，触发就退出该应用
    connect(mExitAct, &QAction::triggered, this, [=]() { qApp->exit(0); });

    InitCityMap();
    weaType();
    //网络请求
    mNetAccessManager = new QNetworkAccessManager(this);
    connect(mNetAccessManager,&QNetworkAccessManager::finished,this,&MainWindow::onReplied);

    //直接在构造中请求天气数据
    //目前默认为阜阳，可更改
    getWeatherInfo("阜阳");

    //给标签添加事件过滤器
    ui->lblHighCurve->installEventFilter(this);
    ui->lblLowCurve->installEventFilter(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::contextMenuEvent(QContextMenuEvent *event)
{
    //弹出右键菜单
    mExitMenu->exec(QCursor::pos());
    event->accept();
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        isLeftPressed = true;
        m_movePoint = event->globalPosition().toPoint();

        //qDebug()<<m_movePoint<<"按下";
        //setCursor(Qt::SizeAllCursor);
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if(!event->buttons().testFlag(Qt::LeftButton))
    {
        return ;
    }
    //qDebug()<<"pos:"<<pos()<<' '<<"globalPos:"<<event->globalPos();
    const QPoint position = pos() + event->globalPos() - m_movePoint;
    //qDebug()<<position<<"移动";
    move(position.x(), position.y());
    m_movePoint = event->globalPos();
}

void MainWindow::InitCityMap()
{
    QString filePath = R"(F:/qtlearning/projects/weather/Citydate.json)";
    QFile file(filePath);
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QByteArray json = file.readAll();
    file.close();
    //qDebug()<<json;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(json,&err); //得到Json文档对象
    //qDebug()<<doc;
    if(err.error != QJsonParseError::NoError){
        return;
    }
    if(!doc.isArray()){
        return;
    }
    QJsonArray cities = doc.array();
    for(int i=0; i<cities.size(); i++)
    {
        //"cityName"这个key要根据你的Json文件中是什么来写
        QString city = cities[i].toObject().value("cityName").toString();
        QString code = cities[i].toObject().value("cityCode").toString();
        if(code.size()>0)
        {
            mCityMaps.insert(city,code);
        }
    }


    //qDebug()<<city;
    //qDebug()<<cities[0];
}

QString MainWindow::getCityCode(QString cityname)
{
    auto it = mCityMaps.find(cityname);
    if(it != mCityMaps.end()){
        //qDebug()<<cityname<<":"<<it.value();
        return it.value();
    }
    return "";
}

void MainWindow::getWeatherInfo(QString cityName)
{
    QString cityCode = getCityCode(cityName);
    if(cityCode.isEmpty())
    {
        QMessageBox::information(this,"提示","请检查城市名是否输错，该页面只支持国内。");
        return ;
    }
    QUrl url("http://t.weather.itboy.net/api/weather/city/" + cityCode);
    mNetAccessManager->get(QNetworkRequest(url));
}

void MainWindow::parseJson(QByteArray &byteArray)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(byteArray,&err);  // 检测json格式
    if(err.error != QJsonParseError::NoError){    // Json格式错误
        return;
    }
    //qDebug()<<doc;
    QJsonObject rootObj = doc.object();

    //解析日期和城市
    mToday.date = rootObj.value("date").toString();
    mToday.city = rootObj.value("cityInfo").toObject().value("city").toString();
    int index = mToday.city.indexOf("市");
    QString result = mToday.city.left(index); // 取出 "市" 前面的子串
    mToday.city = result;
    //qDebug()<<mToday.date<<" "<<mToday.city;

    //解析昨天的
    QJsonObject objData = rootObj.value("data").toObject(); //以{}来判断要不要转换成QJsonObject
    QJsonObject objYesterday = objData.value("yesterday").toObject();

    mDay[0].week = objYesterday.value("week").toString();
    mDay[0].date = objYesterday.value("ymd").toString();
    mDay[0].type = objYesterday.value("type").toString();

    QString s;
    s = objYesterday.value("high").toString().split(" ").at(1);
    s = s.left(s.length() - 1);
    mDay[0].high = s.toInt();

    s = objYesterday.value("low").toString().split(" ").at(1);
    s = s.left(s.length() - 1);
    mDay[0].low = s.toInt();

    //风向风力
    mDay[0].fx = objYesterday.value("fx").toString();
    mDay[0].fl = objYesterday.value("fl").toString();
    //空气质量指数
    mDay[0].aqi = objYesterday.value("aqi").toInt();

    //解析预报中的5天数据
    QJsonArray forecatArr = objData.value("forecast").toArray();
    for(int i = 0;i < 5;i++){
        QJsonObject objForecast = forecatArr[i].toObject();
        mDay[i + 1].week = objForecast.value("week").toString();
        mDay[i + 1].date = objForecast.value("ymd").toString();
        //天气类型
        mDay[i + 1].type = objForecast.value("type").toString();

        QString s;
        s = objForecast.value("high").toString().split(" ").at(1);
        //qDebug()<<s;
        s = s.left(s.length() - 1);
        mDay[i + 1].high = s.toInt();

        s = objForecast.value("low").toString().split(" ").at(1);
        s = s.left(s.length() - 1);
        mDay[i + 1].low = s.toInt();

        //风向风力
        mDay[i + 1].fx = objForecast.value("fx").toString();
        mDay[i + 1].fl = objForecast.value("fl").toString();
        //空气质量指数
        mDay[i + 1].aqi = objForecast.value("aqi").toInt();
    }

    //解析今天的数据
    mToday.ganmao = objData.value("ganmao").toString();
    mToday.wendu = objData.value("wendu").toString().toInt();
    mToday.shidu = objData.value("shidu").toString();
    mToday.pm25 = objData.value("pm25").toInt();
    mToday.quality = objData.value("quality").toString();
    //forecast 中的第一个数组元素，即今天的数据
    mToday.type = mDay[1].type;

    mToday.fx = mDay[1].fx;
    mToday.fl = mDay[1].fl;

    mToday.high = mDay[1].high;
    mToday.low = mDay[1].low;
    //更新UI
    updateUI();

    //绘制最高最低温度曲线
    ui->lblHighCurve->update();
    ui->lblLowCurve->update();
}

void MainWindow::updateUI()
{
    ui->lblData->setText(QDateTime::fromString(mToday.date,"yyyyMMdd").toString("yyyy/MM/dd")
                         + "  " + mDay[1].week);
    ui->lblCity->setText(mToday.city);
    //更新今天的信息
    ui->lblTypeIcon->setPixmap(mTypeMap[mToday.type]);
    ui->lblTemp->setText(QString::number(mToday.wendu)+"°");

    ui->lblType->setText(mToday.type);
    ui->lblLowHigh->setText(QString::number(mToday.low) + "~"
                            + QString::number(mToday.high) + "°C");

    ui->lblGanMao->setText("感冒指数:"+mToday.ganmao);
    ui->lblWindFx->setText(mToday.fx);
    ui->lblWindFl->setText(mToday.fl);
    ui->lblPm25->setText(QString::number(mToday.pm25));
    ui->lblShiDu->setText(mToday.shidu);
    ui->lblQuality->setText(mToday.quality);
    //qDebug()<<mWeekList.size();
    //更新六天的天气状况
    for(int i=0; i<6; i++)
    {
        //更新日期和时间
        mWeekList[i]->setText(mDay[i].week);
        //qDebug()<<mDay[i].week.right(1);
        ui->lblWeek0->setText("昨天");
        ui->lblWeek1->setText("今天");
        ui->lblWeek2->setText("明天");
        QStringList ymdList = mDay[i].date.split("-");
        mDateList[i]->setText(ymdList[1] + "/" + ymdList[2]);
        //qDebug()<<ymdList[0];

        //更新天气类型
        mTypeList[i]->setText(mDay[i].type);
        mTypeIconList[i]->setPixmap(mTypeMap[mDay[i].type]);

        //更新空气质量
        if(mDay[i].aqi >0 && mDay[i].aqi <= 50){
            mAqiList[i]->setText("优");
            mAqiList[i]->setStyleSheet("background-color: rgb(139,195,74);");
        }else if(mDay[i].aqi > 50 && mDay[i].aqi <= 100){
            mAqiList[i]->setText("良");
            mAqiList[i]->setStyleSheet("background-color: rgb(255,170,0);");
        }else if(mDay[i].aqi > 100 && mDay[i].aqi <= 150){
            mAqiList[i]->setText("轻度");
            mAqiList[i]->setStyleSheet("background-color: rgb(255,87,97);");
        }else if(mDay[i].aqi > 150 && mDay[i].aqi <= 200){
            mAqiList[i]->setText("中度");
            mAqiList[i]->setStyleSheet("background-color: rgb(255,17,27);");
        }else if(mDay[i].aqi > 150 && mDay[i].aqi <= 200){
            mAqiList[i]->setText("重度");
            mAqiList[i]->setStyleSheet("background-color: rgb(170,0,0);");
        }else{
            mAqiList[i]->setText("严重");
            mAqiList[i]->setStyleSheet("background-color: rgb(110,0,0);");
        }

        //更新风力、风向
        mFxList[i]->setText(mDay[i].fx);
        mFlList[i]->setText(mDay[i].fl);
    }


}

void MainWindow::weaType()
{
    ui->btnSearch->setShortcut(Qt::Key_Enter);
    ui->btnSearch->setShortcut(Qt::Key_Return);

    //将控件添加到控件数组
    mWeekList << ui->lblWeek0 << ui->lblWeek1 << ui->lblWeek2 << ui->lblWeek3 << ui->lblWeek4 << ui->lblWeek5;
    mDateList << ui->lblDate0 << ui->lblDate1 << ui->lblDate2 << ui->lblDate3 << ui->lblDate4 << ui->lblDate5;

    mTypeList << ui->lblType0 << ui->lblType1 << ui->lblType2 << ui->lblType3 << ui->lblType4 << ui->lblType5;
    mTypeIconList << ui->lblTypeIcon0 << ui->lblTypeIcon1 << ui->lblTypeIcon2 << ui->lblTypeIcon3 << ui->lblTypeIcon4 << ui->lblTypeIcon5;

    mAqiList << ui->lblQuality0 << ui->lblQuality1 << ui->lblQuality2 << ui->lblQuality3 << ui->lblQuality4 << ui->lblQuality5;

    mFxList << ui->lblFx0 << ui->lblFx1 << ui->lblFx2 << ui->lblFx3 << ui->lblFx4 << ui->lblFx5;
    mFlList << ui->lblFl0 << ui->lblFl1 << ui->lblFl2 << ui->lblFl3 << ui->lblFl4 << ui->lblFl5;
    //天气对应图标
    mTypeMap.insert("暴雪",":/img/type/BaoXue.png");
    mTypeMap.insert("暴雨",":/img/type/BaoYu.png");
    mTypeMap.insert("暴雨到暴雪",":/img/type/BaoYuDaoDaBaoYu.png");
    mTypeMap.insert("大暴雨",":/img/type/DaBaoYu.png");
    mTypeMap.insert("大暴雨到大暴雪",":/img/type/DaBaoYuDaoTeDaBaoYu.png");
    mTypeMap.insert("大到暴雪",":/img/type/DaDaoBaoXue.png");
    mTypeMap.insert("大到暴雨",":/img/type/DaDaoBaoYu.png");
    mTypeMap.insert("大雪",":/img/type/DaXue.png");
    mTypeMap.insert("大雨",":/img/type/DaYu.png");
    mTypeMap.insert("冻雨",":/img/type/DongYu.png");
    mTypeMap.insert("多云",":/img/type/DuoYun.png");
    mTypeMap.insert("浮尘",":/img/type/FuChen.png");
    mTypeMap.insert("雷阵雨",":/img/type/LeiZhenYu.png");
    mTypeMap.insert("雷阵雨伴有冰雹",":/img/type/LeiZhenYuBanYouBingBao.png");
    mTypeMap.insert("霾",":/img/type/Mai.png");
    mTypeMap.insert("强沙尘暴",":/img/type/QiangShaChenBao.png");
    mTypeMap.insert("晴",":/img/type/Qing.png");
    mTypeMap.insert("沙尘暴",":/img/type/ShaChenBao.png");
    mTypeMap.insert("特大暴雨",":/img/type/TeDaBaoYu.png");
    mTypeMap.insert("雾",":/img/type/Wu.png");
    mTypeMap.insert("小到中雨",":/img/type/XiaoDaoZhongYu.png");
    mTypeMap.insert("小到中雪",":/img/type/XiaoDaoZhongXue.png");
    mTypeMap.insert("小雪",":/img/type/XiaoXue.png");
    mTypeMap.insert("小雨",":/img/type/XiaoYu.png");
    mTypeMap.insert("雪",":/img/type/Xue.png");
    mTypeMap.insert("扬沙",":/img/type/YangSha.png");
    mTypeMap.insert("阴",":/img/type/Yin.png");
    mTypeMap.insert("雨",":/img/type/Yu.png");
    mTypeMap.insert("雨夹雪",":/img/type/YuJiaXue.png");
    mTypeMap.insert("阵雨",":/img/type/ZhenYu.png");
    mTypeMap.insert("阵雪",":/img/type/ZhenXue.png");
    mTypeMap.insert("中雨",":/img/type/ZhongYu.png");
    mTypeMap.insert("中雪",":/img/type/ZhongXue.png");

}

//消息过滤、主要用于重绘子控件，过滤Paint事件
//使用eventFilter步骤，1:对目标对象调用installEventFilter()来注册监视对象 2:重写eventFilter()函数处理目标对象
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    //绘制最高温度的曲线
    if(obj == ui->lblHighCurve && event->type() == QEvent::Paint)
    {
        paintHighCurve();
    }
    //绘制低温的曲线
    if(obj == ui->lblLowCurve && event->type() == QEvent::Paint)
    {
        paintLowCurve();
    }
    return QWidget::eventFilter(obj,event);
}

void MainWindow::paintHighCurve()
{
    QPainter painter(ui->lblHighCurve);
    //抗锯齿
    painter.setRenderHint(QPainter::Antialiasing,true);

    //获取x坐标
    int pointX[6] = {0};
    for(int i = 0;i < 6;i++){
        pointX[i] = mWeekList[i]->pos().x() + mWeekList[i]->width() / 2;
    }

    int tmpSum = 0;
    int tmpAvg = 0;
    for(int i = 0;i < 6;i++){
        tmpSum += mDay[i].high;
    }
    tmpAvg = tmpSum / 6;

    //获取y坐标
    int pointY[6] = {0};
    int yCenter = ui->lblHighCurve->height() / 2;
    for(int i = 0;i < 6;i++){
        pointY[i] = yCenter - ((mDay[i].high - tmpAvg) * INCREMENT);
    }

    //绘制
    QPen pen = painter.pen();
    pen.setWidth(1);
    pen.setColor(QColor(255,170,0));
    painter.setPen(pen);
    painter.setBrush(QColor(255,170,0));    //设置画刷内部填充的颜色

    //画点、写文本
    for(int i = 0;i < 6;i++){
        painter.drawEllipse(QPoint(pointX[i],pointY[i]),POINT_RADIUS,POINT_RADIUS);
        //显示温度文本
        painter.drawText(pointX[i] - TEXT_OFFSET_X,pointY[i] - TEXT_OFFSET_Y,
                         QString::number(mDay[i].high) + "°");
    }
    for(int i = 0;i < 5;i++){
        if(i == 0){
            pen.setStyle(Qt::DashLine);
            painter.setPen(pen);
        }else{
            pen.setStyle(Qt::SolidLine);
            painter.setPen(pen);
        }
        painter.drawLine(pointX[i],pointY[i],pointX[i + 1],pointY[i + 1]);
    }
}

void MainWindow::paintLowCurve()
{
    QPainter painter(ui->lblLowCurve);

    // 抗锯齿
    painter.setRenderHint(QPainter::Antialiasing,true);

    //获取x坐标
    int pointX[6] = {0};
    for(int i = 0;i < 6;i++){
        pointX[i] = mWeekList[i]->pos().x() + mWeekList[i]->width() / 2;
    }

    //获取y坐标
    int pointY[6] = {0};
    int yCenter = ui->lblLowCurve->height() / 2;

    //求平均气温
    int tmpSum = 0;
    int tmpAvg = 0;
    for(int i = 0;i < 6;i++){
        tmpSum += mDay[i].low;
    }
    tmpAvg = tmpSum / 6;

    for(int i = 0;i < 6;i++){
        pointY[i] = yCenter - ((mDay[i].low - tmpAvg) * INCREMENT);
    }

    //绘制
    QPen pen = painter.pen();
    pen.setWidth(1);
    pen.setColor(QColor(0,255,255));

    painter.setPen(pen);
    painter.setBrush(QColor(0,255,255));    //设置画刷内部填充的颜色

    //画点、写文本
    for(int i = 0;i < 6;i++){
        painter.drawEllipse(QPoint(pointX[i],pointY[i]),POINT_RADIUS,POINT_RADIUS);
        //显示温度文本
        painter.drawText(pointX[i] - TEXT_OFFSET_X,pointY[i] - TEXT_OFFSET_Y,QString::number(mDay[i].low) + "°");
    }
    for(int i = 0;i < 5;i++){
        if(i == 0){
            pen.setStyle(Qt::DashLine);
            painter.setPen(pen);
        }else{
            pen.setStyle(Qt::SolidLine);
            painter.setPen(pen);
        }
        painter.drawLine(pointX[i],pointY[i],pointX[i + 1],pointY[i + 1]);
    }
}

void MainWindow::onReplied(QNetworkReply *reply)
{
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if(reply->error() != QNetworkReply::NoError || statusCode!=200)
    {
        qDebug() << reply->errorString().toLatin1().data();
        QMessageBox::warning(this,"天气","请求数据失败",QMessageBox::Ok);
    }
    else{
        QByteArray  byteArray = reply->readAll();
                //qDebug() << "读所有：" << byteArray.data();
        parseJson(byteArray);
    }
    reply->deleteLater();
}



void MainWindow::on_btnSearch_clicked()
{
    QString cityName = ui->cityEdit->text();
    if(cityName == ""){
        return;
    }
    getWeatherInfo(cityName);
}

