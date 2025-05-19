#include "mainwindow.h"
#include "authwidget.h"
#include "ui_mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <cstdlib>
#include "server_ip.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QMessageBox>
#include <QNetworkReply>
#include <QEventLoop>
#include <QJsonArray>
#include <QTimer>
#include <QString>
#include <QWidget>
#include <QNetworkAccessManager>
#include <QJsonValue>

// конструктор
MainWindow::MainWindow(QWidget *parent): QMainWindow(parent), ui(new Ui::MainWindow) {

    // настройка UI
    ui->setupUi(this);

    // инициализация виджетов доступа в систему (авторизация/регистрация)
    stackedWidget = new QStackedWidget(this); // контейнер для переключения между виджетами
    setCentralWidget(stackedWidget); // отображение всего содержимого внутри контейнера
    authWidget = new AuthWidget(); // инициализация виджета авторизации
    mainContent = new QWidget(); // виджет содержимого

    QVBoxLayout *mainLayout = new QVBoxLayout(mainContent); // вертикальный макет для контента
    mainLayout->addWidget(ui->textBrowser); // вывод информации
    QHBoxLayout *inputLayout = new QHBoxLayout(); // горизонтальный макет
    inputLayout->addWidget(ui->lineEdit); // поле ввода
    inputLayout->addWidget(ui->pushButton); // кнопка
    mainLayout->addLayout(inputLayout); // сведение макетов
    // подключение виджетов в макет
    stackedWidget->addWidget(authWidget); // подключение виджета как основного для пользователя
    stackedWidget->addWidget(mainContent); // подключение как дополнительного

    // блок подключения сигналов к виджету
    connect(
        authWidget,
        &AuthWidget::loginSuccess,
        this,
        &MainWindow::handleLoginSuccess
        );
    connect(
        authWidget,
        &AuthWidget::registerSuccess,
        this,
        &MainWindow::handleRegisterSuccess
        );
    connect(
        ui->lineEdit,
        &QLineEdit::returnPressed,
        ui->pushButton,
        &QPushButton::click
        );
}

// функция получения ответа от ИИ модели
QJsonArray MainWindow::ai_answer(QString question)
{
    // инициализация обработчиков api
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(ip + "chat"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject json; // инициализация обработчика JSON
    json.insert("token", authWidget->getAuthToken()); // вставка токена в JSON
    json.insert("question", question); // вставка текста запроса пользователя к ИИ в JSON

    // отправка запроса
    QNetworkReply* reply = manager.post(request, QJsonDocument(json).toJson());
    // инициализация цикла ожидания ответа от сервера ("петля событий")
    QEventLoop eventLoop;
    QObject::connect(
        reply,
        &QNetworkReply::finished,
        &eventLoop,
        &QEventLoop::quit
        );
    eventLoop.exec(); // запуск петли

    // инициализация истории переписки с ИИ
    QJsonArray history;
    // получение ответа от сервера
    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject response = doc.object();
        if(response.contains("history") && response["history"].isArray()) {
            history = response["history"].toArray();
        }
    }
    reply->deleteLater();
    return history;
}

// обработка успешной авторизации
void MainWindow::handleLoginSuccess(const QString username, const QString password, const QJsonArray& history)
{
    // блокировка интерфейса
    ui->lineEdit->setEnabled(false);
    ui->pushButton->setEnabled(false);

    ui->textBrowser->clear(); // очистка поля с историей
    displayHistory(history); // отображение истории

    // разблокировка интерфейса
    ui->lineEdit->setEnabled(true);
    ui->pushButton->setEnabled(true);
    stackedWidget->setCurrentIndex(1); // переход к основному окну
}

// обработка успешной регистрации
void MainWindow::handleRegisterSuccess()
{
    authWidget->switchToLogin(); // возврат к авторизации
}

// обработка нажатия кнопки "отправить"
void MainWindow::on_pushButton_clicked()
{
    // считывание запроса
    QString question = ui->lineEdit->text();
    if(question.isEmpty()) return;

    // сохранение + обновление истории
    QString currentHistory = ui->textBrowser->toHtml();
    appendToTextBrowser("Вы:", question, "black");
    // блокировка интерфейса
    ui->lineEdit->setText("Генерация ответа...");
    ui->lineEdit->setEnabled(false);
    ui->pushButton->setEnabled(false);

    // отправка запроса на сервер
    QTimer::singleShot(0, [this, question, currentHistory]() {
        QJsonArray history = ai_answer(question);

        // апдейт истории
        ui->textBrowser->clear();
        ui->textBrowser->setHtml(currentHistory);
        displayHistory(history);

        // разблокировка интерфейса
        ui->lineEdit->setEnabled(true);
        ui->pushButton->setEnabled(true);
        ui->lineEdit->clear();
    });
}


/*
 * какая-то версия нерабочая
 *
void MainWindow::on_pushButton_clicked()
{
    QString question = ui->lineEdit->text();
    if (question.isEmpty()) return;

    ui->lineEdit->setText("Генерация ответа...");
    ui->lineEdit->setEnabled(false);
    ui->pushButton->setEnabled(false);

    appendToTextBrowser("Вы:", question, "black");

    QString answer = (question);

    // Append the AI's response directly without clearing the text browser
    appendToTextBrowser("AI:", answer, "blue");

    ui->lineEdit->setEnabled(true);
    ui->pushButton->setEnabled(true);
    ui->lineEdit->clear();
}
*
*/


// вставка текста в поле
void MainWindow::appendToTextBrowser(
    const QString& label,
    const QString& message,
    const QString& color) {
    // стили
    QString formatted = QString(
        "<div style='margin:8px 0; padding:5px; color:%3;'>"
        "<b>%1</b> %2"
        "</div>"
        )
        .arg(label)
        .arg(message)
        .arg(color);

    ui->textBrowser->append(formatted);
}

// функция обновления истории (чтоб timout error от сервера починить)
void MainWindow::displayHistory(const QJsonArray& history)
{
    ui->textBrowser->clear(); // очистка старой истории

    for(const QJsonValue& entry : history) {
        QJsonObject message = entry.toObject();
        QString role = message["role"].toString();
        // пропуск системного промпта
        if(role == "system") {
            continue;
        }
        QString content = message["content"].toString();
        QString color = role == "user" ? "black" : "blue";
        appendToTextBrowser(role == "user" ? "Вы:" : "AI:", content, color);
    }
}
// a

// деструктор
MainWindow::~MainWindow()
{
    delete ui;
}
