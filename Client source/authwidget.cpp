#include "authwidget.h"
#include "ui_authwidget.h"
#include "server_ip.h"
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// конструктор
AuthWidget::AuthWidget(QWidget *parent):
    QWidget(parent), ui(new Ui::AuthWidget), networkManager(new QNetworkAccessManager(this)) {

    // настройка UI
    ui->setupUi(this);
    ui->stackedWidget->setCurrentIndex(0); // начинаем с авторизации
    // блок подключения сигналов к виджету
    connect(
        ui->btnLogin,
        &QPushButton::clicked,
        this,
        &AuthWidget::onLoginClicked
        );
    connect(
        ui->btnRegister,
        &QPushButton::clicked,
        this,
        &AuthWidget::onRegisterClicked
        );
    connect(
        ui->btnSwitchToRegister,
        &QPushButton::clicked,
        this,
        &AuthWidget::switchToRegister
        );
    connect(
        ui->btnSwitchToLogin,
        &QPushButton::clicked,
        this,
        &AuthWidget::switchToLogin
        );
}

// функция получения токена аутентификации для доступа к запросам
QString AuthWidget::getAuthToken() const {
    return authToken;
}

// обработка авторизации
void AuthWidget::onLoginClicked()
{
    // получение логина и пароля
    QString username = ui->lineEditLogin->text();
    QString password = ui->lineEditPassword->text();

    if(username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите логин и пароль!");
        return;
    }

    // формировка файла JSON для авторизации
    QJsonObject data{{"username", username}, {"password", password}};
    // отправка запроса на авторизацию по настроенному домену
    QNetworkRequest request(QUrl(ip + "login")); //
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // получение ответа от сервера в формате JSON
    QNetworkReply* reply = networkManager->post(request, QJsonDocument(data).toJson());

    // запись JSON ответа от сервера в переменные
    // подключение обработчика ошибок
    connect(reply, &QNetworkReply::finished, [=]() {
        if(reply->error() != QNetworkReply::NoError) {
            QMessageBox::critical(this, "Ошибка", "Ошибка сети: " + reply->errorString());
            reply->deleteLater();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if(doc.isObject()) {
            QJsonObject response = doc.object();
            authToken = response["token"].toString();
            QJsonArray history = response["history"].toArray();
            emit loginSuccess(username, password, history);
        }

        reply->deleteLater();
    }
    );
}

// обработка авторизации
void AuthWidget::onRegisterClicked()
{
    // считывание вводимых данных
    QString username = ui->lineEditNewLogin->text();
    QString password = ui->lineEditNewPassword->text(); // чтобы звёздочки видно было
    QString confirm = ui->lineEditConfirmPassword->text(); // аналогично

    // проверка пустых полей
    if(username.isEmpty() || password.isEmpty() || confirm.isEmpty() || password != confirm) {
        QMessageBox::warning(this, "Ошибка", "Заполните все поля!");
        return;
    }

    // формировка файла JSON для регистрации
    QJsonObject data;
    data[username] = QJsonObject{{"password", password}};

    // отправка запроса на регистрацию по настроенному домену
    QNetworkRequest request(QUrl(ip + "register"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // получение ответа от сервера в формате JSON
    QNetworkReply* reply = networkManager->post(request, QJsonDocument(data).toJson());

    // запись JSON ответа от сервера в переменные
    connect(reply, &QNetworkReply::finished, [=]() {
        if(reply->error() != QNetworkReply::NoError) {
            QMessageBox::critical(this, "Ошибка", "Ошибка регистрации: " + reply->errorString());
            reply->deleteLater();
            return;
        }

        emit registerSuccess();
        reply->deleteLater();
    });
}

// функция переключения к окну авторизации
void AuthWidget::switchToLogin()
{
    ui->stackedWidget->setCurrentIndex(0);
}

// функция переключения к окну регистрации
void AuthWidget::switchToRegister()
{
    ui->stackedWidget->setCurrentIndex(1);
}

// деструктор
AuthWidget::~AuthWidget() { delete ui; }
