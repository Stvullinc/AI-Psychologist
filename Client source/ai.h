#ifndef AI_H
#define AI_H

#include <QStackedWidget>
#include "authwidget.h"
#include <QMessageBox>
#include <QWidget>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QNetworkReply>

QJsonArray ai_answer(QString question);

#endif // AI_H
