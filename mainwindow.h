#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QSettings>

#include <future>

#include <PCANComm.h>
#include <Common.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/**
 * @brief PCAN Channel Combobox 클릭 시 처리할 Handler Class
 *
 */
class PCANChannelComboboxHandle : public QObject {
    Q_OBJECT
public:
    explicit PCANChannelComboboxHandle(QObject *parent = nullptr);
    virtual ~PCANChannelComboboxHandle() override;
    static void addPCANChanneltoComboBox(QComboBox* pComboBox);
protected:
    bool eventFilter(QObject* object, QEvent* event) override;
};


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    Ui::MainWindow *ui;
    CANSettingParam settingParam;

    PCANComm* comm;
    QQueue<cantp_msg>* readMsgQueue;
    bool bIsRunning;
    std::future<void> readDataFuture;
    std::future<void> readQueueFuture;

    void initForm();
    void initConnect();
    void loadCANSettingParam();

    void writeMessage();

    void writeSetting(const QString& sKey, const QVariant& qValue);
    QVariant getSetting(const QString& sKey);
};
#endif // MAINWINDOW_H
