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

    // 十分钟
    startTimer(10*60*1000);
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
    else if(cmd == HC_QUERY)
    {
        respObj = handleQuery(obj);
    }

    rep.end(QJsonDocument(respObj).toJson());
    return;
}

/*
    cmd: query,
    object: tuser,
    username: xxxx,
    password: yyy
    codition:xxx
    condition:yyy

*/
QJsonObject MyServer::handleQuery(QJsonObject obj)
{
    QString object = obj.value(HC_OBJECT).toString(); // tuser
    obj.remove(HC_CMD);
    obj.remove(HC_OBJECT);

    QStringList keys = obj.keys();
    QStringList filters; // username='xxx'   password='yyy'
    foreach (QString key, keys) {
        filters += QString("%1='%2'").arg(key).arg(obj.value(key).toString());
    }

    QString filter = filters.join(" and ");

    QSqlTableModel model;
    model.setTable(object);

    model.setFilter(filter); // where子句
    model.select(); // 最多只select 一个记录

    QJsonArray arr;
    QSqlRecord record = model.record();
    for(int row=0; row<model.rowCount(); ++row)
    {
        QJsonObject tmp;
        for(int col=0; col<record.count(); ++col)
        {
            tmp.insert(record.fieldName(col), model.data(model.index(row, col)).toString());
        }
        arr.append(tmp);
    }

    QJsonObject respObject;
    respObject.insert(HC_RESULT, HC_OK);
    respObject.insert(HC_COUNT, model.rowCount());
    respObject.insert(HC_DATA, arr);

    return respObject;
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
    if(type == HC_TEMP)
    {
        respObj = handleInsertT(obj);
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

QJsonObject MyServer::handleInsertT(QJsonObject obj)
{
    QJsonObject resp;
    resp.insert(HC_RESULT, HC_OK);
    /*
                insertObj.insert(HC_CMD, HC_INSERT);
                insertObj.insert(HC_OBJECT, HC_SESSION);
                insertObj.insert(HC_USERNAME, HC_USERNAME);
                insertObj.insert(HC_SESSION, uuid);// session怎么产生
                insertObj.insert(HC_LOGINTYPE, type);
                insertObj.insert(HC_TYPE, HC_TEMP);
*/
    QString object = obj.value(HC_OBJECT).toString();
    if(object == HC_SESSION)
    {
        QString session = obj.value(HC_SESSION).toString();
        QString username = obj.value(HC_USERNAME).toString();
        QString loginType = obj.value(HC_LOGINTYPE).toString();

        UserInfo* info = _sessions[session];
        if(info == NULL)
        {
            info = new UserInfo;
            _sessions.insert(session, info);
            info->username = username;
            info->type = loginType;
            info->lat = 0;
            info->lng = 0;
            info->session = session;
            info->tickCounter = 0;

            qDebug() << "insert session " << session << username;
        }
        else
        {
            info->tickCounter = 0;
            info->username = username;
            info->session = session;

            qDebug() << "modify session " << session << username;
        }

        qDebug() << "session count is" << _sessions.count();
    }
    return resp;
}

//
void MyServer::timerEvent(QTimerEvent *)
{
    for(auto it = _sessions.begin(); it != _sessions.end();)
    {
        UserInfo* info = it.value();
        info->tickCounter++;
        if(info->tickCounter >= 6)
        {
            delete info;
            it = _sessions.erase(it);
        }
        else
        {
            ++it;
        }
    }
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
