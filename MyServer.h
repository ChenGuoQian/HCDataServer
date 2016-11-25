#ifndef MYSERVER_H
#define MYSERVER_H

#include <QObject>
#include <tufao-1/Tufao/HttpServer>
#include <tufao-1/Tufao/HttpServerRequest>
#include <tufao-1/Tufao/HttpServerResponse>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
using namespace Tufao;

/*
    负责数据的存储（插入）、读取、更改

    insert [type表示持久存储、临时存储] [对象] [data....]
    update
    delete
    get

    insert
    {
        cmd: insert,
        type: p,
        obj: tuser,
        data [
            xiaoming,
            kkk,
            133,
            aa@aa.bb,
            12312312312321
        ]
    }
*/
class MyServer : public QObject
{
    Q_OBJECT
public:
    explicit MyServer(QObject *parent = 0);

    Tufao::HttpServer* _server;

    void handle(HttpServerRequest& req, HttpServerResponse& rep);
    QJsonObject handleInsert(QJsonObject obj);
    QJsonObject handleInsertP(QJsonObject obj);
signals:

public slots:
};

#endif // MYSERVER_H
