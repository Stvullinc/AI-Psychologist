#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include "authwidget.h"
#include <QMessageBox>
#include <QWidget>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QNetworkReply>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pushButton_clicked();
    void handleRegisterSuccess();
    void handleLoginSuccess(const QString username, const QString password, const QJsonArray& history);

private:
    Ui::MainWindow *ui;
    QStackedWidget *stackedWidget;
    AuthWidget *authWidget;
    QWidget *mainContent;
    QJsonArray ai_answer(QString question);
    void displayHistory(const QJsonArray& history);
    void appendToTextBrowser(
        const QString& label,
        const QString& message,
        const QString& color
        );
};

#endif // MAINWINDOW_H
