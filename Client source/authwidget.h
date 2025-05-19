#ifndef AUTHWIDGET_H
#define AUTHWIDGET_H

#include <QWidget>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QNetworkReply>

QT_BEGIN_NAMESPACE
namespace Ui { class AuthWidget; }
QT_END_NAMESPACE

class AuthWidget : public QWidget
{
    Q_OBJECT

private:
    QString authToken;

public:
    explicit AuthWidget(QWidget *parent = nullptr);
    QString getAuthToken() const;
    void switchToLogin();
    ~AuthWidget();

signals:
    void registerSuccess();
    void loginSuccess(const QString username, const QString password, const QJsonArray& history);

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void switchToRegister();

private:
    Ui::AuthWidget *ui;
    QNetworkAccessManager *networkManager;

};

#endif // AUTHWIDGET_H
