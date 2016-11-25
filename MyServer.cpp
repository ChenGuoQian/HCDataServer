#include "MyServer.h"
#include <QCoreApplication>
#include "../HCServer/Def.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlTableModel>
#include <QSqlError>
#include <QSqlRecord>
#include <QSqlField>

MyServer::MyServer(QObject *parent) : QObject(parent)
{
    _server = new HttpServer;
    connect(_server, &HttpServer::requestReady, [&](HttpServerRequest& req, HttpServerResponse& rep){
        connect(&req, &HttpServerRequest::end, [&](){
            handle(req, rep);
        });
    });

    if(!_server->listen(QHostAddress::Any, 10252))
    {
        qDebug() << "listen error";
        exit(2);
    }
}

void MyServer::handle(HttpServerRequest &req, HttpServerResponse &rep)
{
    rep.writeHead(HttpResponseStatus::OK);

    QByteArray buf = req.readBody();
    QJsonObject obj = QJsonDocument::fromJson(buf).object();

    QJsonObject respObj;
    respObj.insert(HC_RESULT, HC_ERR);
    respObj.insert(HC_REASON, HC_UNKNOWN);

    QString cmd = obj.value(HC_CMD).toString();
    if(cmd == HC_INSERT)
    {
        respObj = handleInsert(obj);
    }

    rep.end(QJsonDocument(respObj).toJson());
    return;
}

QJsonObject MyServer::handleInsert(QJsonObject obj)
{
    QString type = obj.value(HC_TYPE).toString();
    QJsonObject respObj;
    respObj.insert(HC_RESULT, HC_ERR);
    respObj.insert(HC_REASON, HC_UNKNOWN);

    if(type == HC_PERMANENT)
    {
        respObj = handleInsertP(obj);
    }

    return respObj;
}

QJsonObject MyServer::handleInsertP(QJsonObject obj)
{
    QJsonObject respObj;
    respObj.insert(HC_RESULT, HC_OK);

    // 数据表
    QString table = obj.value(HC_OBJECT).toString(); // tuser

    // 需要插入的数据
    QJsonArray arr = obj.value(HC_DATA).toArray();

    // 定义了数据表的model
    QSqlTableModel model;
    model.setTable(table); // table = tuser

    // 获得表的记录信息，record中记录着字段、字段名、字段数量
    QSqlRecord record = model.record();

    // 在model插入一条数据
    if(!model.insertRow(0))
    {
        respObj.insert(HC_RESULT, HC_ERR);
        respObj.insert(HC_REASON, "insertRow error");
        goto RETURN;
    }
    // 在新增加的记录中，填写数据
    for(int i=0;i<record.count(); ++i)
    {
        model.setData(model.index(0, i), arr[i].toString());
    }

    // 把数据提交到数据库
    // model.submit();
    if(!model.submitAll()) // 提交到数据库
    {
        respObj.insert(HC_RESULT, HC_ERR);
        respObj.insert(HC_REASON, "submit error");
    }

RETURN:
    return respObj;
}


int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName("127.0.0.1");
    db.setUserName("bc");
    db.setPassword("Ab123456_");
    db.setDatabaseName("bc");

    if(!db.open())
    {
        qWarning() << "open database error" << db.lastError().text();
        return 1;
    }

    new MyServer;

    return app.exec();
}
