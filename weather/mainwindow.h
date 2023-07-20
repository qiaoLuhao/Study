#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMouseEvent>
#include <QMenu>
#include <QContextMenuEvent>
#include <QMap>
#include <QLabel>
#include <QtNetwork/QNetworkAccessManager>
#include "weatherdata.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();


protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    //初始化城市与城市码之间的关联、得到城市码
    void InitCityMap();
    QString getCityCode(QString cityname);
    //获取天气数据、解析Json、更新UI
    void getWeatherInfo(QString cityName);
    void parseJson(QByteArray& byteArray);
    void updateUI();
    //
    void weaType();
    //重写父类的eventFilter方法
    bool eventFilter(QObject* obj,QEvent* event);
    void paintHighCurve();
    void paintLowCurve();

private slots:
    void onReplied(QNetworkReply* reply);

    void on_btnSearch_clicked();

private:
    Ui::MainWindow *ui;
    QMenu* mExitMenu;   // 退出菜单
    QAction* mExitAct;  // 菜单项（退出）

    QMap<QString,QString> mCityMaps;
    QNetworkAccessManager* mNetAccessManager;

    QPoint m_movePoint;
    bool isLeftPressed;

    Today mToday;
    Day mDay[6];

    //星期和日期
    QList<QLabel*> mWeekList;
    QList<QLabel*> mDateList;

    //天气和图标
    QList<QLabel*> mTypeList;
    QList<QLabel*> mTypeIconList;

    //质量指数
    QList<QLabel*> mAqiList;

    //风向和风力
    QList<QLabel*> mFxList;
    QList<QLabel*> mFlList;

    QMap<QString,QString> mTypeMap;


};
#endif // MAINWINDOW_H
