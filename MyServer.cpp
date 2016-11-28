#include "MyServer.h"
#include <QCoreApplication>
#include "../HCServer/Def.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlTableModel>
#include <QSqlError>
#include <QSqlRecord>
#include <QSqlField>
#include <QDateTime>


//static char base32encode[] = "0123456789abcdefghijklmnopqrstuvwxyz";
static char base32encode[32];

char *getGeohash(double lng, double lat, char *buf, int bits)
{
    uint64_t lngLatBits = 0;

    double lngMin = -180;
    double lngMax = 180;
    double latMin = -90;
    double latMax = 90;

    for(int i=0; i<bits; ++i)
    {
        lngLatBits<<=1;

        double lngMid = (lngMax + lngMin)/2;
        if(lng > lngMid)
        {
            lngLatBits += 1;
            lngMin = lngMid;
        }
        else
        {
            lngMax = lngMid;
        }

        lngLatBits <<=1;

        double latMid = (latMax + latMin)/2;
        if(lat > latMid)
        {
            lngLatBits += 1;
            latMin = latMid;
        }
        else
        {
            latMax = latMid;
        }
    }



    // 11010 10010 10010 10010 10101 11011 00100 11000
    // a     b     c     1     2     x     2     3    \0
    int i;
    for(i=0;i <8; ++i)
    {
        uint32_t index = lngLatBits >> (35-i*5);
        index &= 31;
        buf[i] = base32encode[index];
    }

    buf[i] = 0;
    return buf;
}

QByteArray getGeohash(double lng, double lat, int bits = 20)
{
    char buf[10]; // 8位的长度经度是偏差是19米，8位长度对应的bits是20，40位整数
    getGeohash(lng, lat, buf, bits);
    return QByteArray(buf);
}

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
    qDebug() << "request buf:" << buf;
    QJsonObject obj = QJsonDocument::fromJson(buf).object();

    QJsonObject respObj;
    respObj.insert(HC_RESULT, HC_ERR);
    respObj.insert(HC_REASON, HC_UNKNOWN);

    QString cmd = obj.value(HC_CMD).toString();

    /*
        {
            cmd: update,
            object: position,
            session: {xxxx-xxxxx-xxxxx-xxxxx},
            lng: 118.19098111,
            lat: 39.11882711
        }
    */
    if(cmd == HC_GET)
    {
        respObj = handleGet(obj);
    }
    else if(cmd == HC_UPDATE)
    {
        respObj = handleUpdate(obj);
    }
    else if(cmd == HC_INSERT)
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
    respObj.insert(HC_RESULT, HC_ERR);

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
        respObj.insert(HC_REASON, "insertRow error");
        goto RETURN;
    }

    if(table == HC_USER_TABLE)
    {
        // 在新增加的记录中，填写数据
        for(int i=0;i<record.count(); ++i)
        {
            model.setData(model.index(0, i), arr[i].toString());
        }
    }
    else if(table == HC_ORDER)
    {
        for(int i=0; i<arr.count(); ++i)
        {
            // QT会自动转，也可能不会自动转，所以可能会出错
            model.setData(model.index(0, i), arr[i].toString());
        }
    }

    // 把数据提交到数据库
    // model.submit();
    if(!model.submitAll()) // 提交到数据库
    {
        respObj.insert(HC_REASON, "submit error");
        qDebug() << "**************************" << "model submitAll error";
        goto RETURN;
    }

    respObj.insert(HC_RESULT, HC_OK); // 已经把数据写入数据库了

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

        UserInfo* info = _users[session];
        if(info == NULL)
        {
            info = new UserInfo;
            _users.insert(session, info);
            info->username = username;
            info->type = loginType;
            info->lat = 0;
            info->lng = 0;
            info->session = session;
            // 获得当前时间戳值，这个值是从1970：1:1:00:00:00
            info->timestamp = QDateTime::currentMSecsSinceEpoch();

            //_lru.append(info);
            // O(1)
            _lru.addTail(&info->node);

            qDebug() << "insert session " << session << username;
        }
        else
        {
            info->timestamp = QDateTime::currentMSecsSinceEpoch();
            info->username = username;
            info->session = session;
            // _lru.removeOne(info);
            // _lru.append(info);
            // O(1)
            _lru.delNode(&info->node);
            _lru.addTail(&info->node);

            qDebug() << "modify session " << session << username;
        }

        qDebug() << "session count is" << _users.count();
    }
    return resp;
}

/*
    {
        cmd: update,
        object: position,
        session: {xxxx-xxxxx-xxxxx-xxxxx},
        lng: 118.19098111,
        lat: 39.11882711
    }
*/
QJsonObject MyServer::handleUpdate(QJsonObject obj)
{
    QJsonObject resp;
    resp.insert(HC_RESULT, HC_ERR);
    if(obj.value(HC_OBJECT).toString() == HC_POSITION)
    {
        QString session = obj.value(HC_SESSION).toString();
        //  UserInfo* info = _users[session];

        // 不能找到用户信息，说明该用户已经好久没有操作，session已经结束
        if(_users.find(session) == _users.end())
        {
            qDebug() << "can not find session";
            resp.insert(HC_REASON, "can not find session");
            return resp;
        }

        double lng = obj.value(HC_LNG).toString().toDouble();
        double lat = obj.value(HC_LAT).toString().toDouble();

        UserInfo* user = _users[session];

        user->lng = lng;
        user->lat = lat;

        QByteArray geohash = getGeohash(lng, lat);
        qDebug() << "new geohash is" << geohash;

        if(user->getHash != geohash)
        {
            // 说明司机正在越界
            GeoNodeLeaf* leafOld = getLeaf(user->getHash);
            GeoNodeLeaf* leafNew = getLeaf(geohash);

            if(leafOld)
                leafOld->users.removeOne(user);

            qDebug() << 1;
            leafNew->users.append(user);

            user->getHash = geohash;
        }

        qDebug() << 2;

        // 客户端向服务器发送数据时，需要重置tickCounter，避免该用户被服务器踢掉
        user->timestamp = QDateTime::currentMSecsSinceEpoch();

        // _lru.removeOne(user);
        // O(1)
        _lru.delNode(&user->node);
        _lru.addTail(&user->node);

        qDebug() << 3;

        // 检测订单表，看该司机是否有订单未处理，如果有，把订单信息返回给司机
        if(user->status == HC_USER_PENDING)
        {
            // 说明要给这个司机发送订单信息
        }

        resp.insert(HC_RESULT, HC_OK);

        qDebug()   <<"Change Location:" << user->username << user->lng << user->lat << user->status << user->type;
    }

    return resp;
}

QJsonObject MyServer::handleGet(QJsonObject obj)
{
    QJsonObject ret;
    ret.insert(HC_RESULT, HC_ERR);

    QString object = obj.value(HC_OBJECT).toString();
    if(object == HC_USERID)
    {
        QString session = obj.value(HC_SESSION).toString();
        if(_users.find(session) == _users.end())
        {
            return ret;
        }

        UserInfo* user = _users[session];
        ret.insert(HC_USERID, user->username);
        ret.insert(HC_RESULT, HC_OK);
    }
    else if(object == HC_NEARBY_DRIVER)
    {
        QString lng = obj.value(HC_LNG).toString();
        QString lat = obj.value(HC_LAT).toString();

        // 中心点的geohash
        QByteArray geohash = getGeohash(lng.toDouble(), lat.toDouble());

        // 搜索范围
        // geohash + 0 geohash +-1 geohash +-2
        int range[] = {0, 1, -1, 2, -2, 3, -3, 4, -4, 5, -5};

        for(uint32_t i=0; i <sizeof(range)/sizeof(range[0]); ++i)
        {
            QByteArray searchHash = geohashAdd( geohash , range[i]);
            GeoNodeLeaf* leaf = getLeaf(searchHash);
            if(leaf == NULL) continue;

            foreach (UserInfo* user, leaf->users)
            {
                if(user->type == "乘客" || user->status == HC_USER_BUSY)
                    continue;

                user->status = HC_USER_BUSY;

                qDebug()   <<"Nearby Driver:" << user->username << user->lng << user->lat << user->status << user->type;


                ret.insert(HC_USERID, user->username);
                ret.insert(HC_RESULT, HC_OK);
                return ret;
            }
        }
    }

    return ret;
}

// 0123456789abcdefghijklmnopqrstuv
QByteArray MyServer::geohashAdd(QByteArray geohash, int data)
{
    // geohash = "AAAA AAAf"  A --> f 相差31
    // ret = "AAAA AABB"
    // data = 2

    int pos = 7;
    QByteArray ret = geohash;

    while(pos >= 0)
    {
        //'f';  'A'
        char cur = geohash.at(pos);

        // 'f+2' = A+33 'B'
        cur += data;

        if(cur < 'A')
        {
            cur += 32;
            data = -1;
            ret[pos] = cur;
            pos -= 1;

        }
        else if(cur >= 'A'+32) //
        {
            cur -= 32; // 'B'
            data = 1;
            ret[pos] = cur;
            pos -= 1;

        }
        else
        {
            ret[pos] = cur;
            break;
        }
    }

    return ret;
}

GeoNodeLeaf *MyServer::getLeaf(QByteArray geohash)
{
    // static char base32encode[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    if(geohash.size() == 0)
        return NULL;

    GeoNode* current = &_head;
    GeoNode* tmp;
    for(int i=0; i<8; ++i)
    {
        int idx;
        char ch = geohash.at(i);
        if(ch <= '9')
            idx = ch-'0';
        else
            idx = ch-'a' + 10;

        tmp = current->child[idx];
        if(tmp == NULL)
        {
            if(i<7)
                tmp = new GeoNode;
            else
                tmp = new GeoNodeLeaf;
            current->child[idx] = tmp;
        }
        current = tmp;
    }

    return (GeoNodeLeaf*)current;
}

//
void MyServer::timerEvent(QTimerEvent *)
{
    quint64 ts = QDateTime::currentMSecsSinceEpoch();

    HCListNode* node = _lru.head.next; // first node
    while(node != &_lru.head)
    {
        // 通过node得到UserInfo，因为node是UserInfo的第一个字段，所以它们的首地址是一样的。
        UserInfo* user = (UserInfo*)node;
        node = node->next;

        if(ts - user->timestamp > 30*60*1000)
        {
            GeoNodeLeaf* leaf = getLeaf(user->getHash);
            if(leaf)
                leaf->users.removeOne(user);

            _users.remove(user->session);
            _lru.delNode(&user->node);

            delete user;
        }
        else
        {
            break;
        }

    }

#if 0
    for(auto it = _lru.begin(); it!= _lru.end();)
    {
        UserInfo* info = *it;
        if(ts - info->timestamp > 30*60*1000)
        {
            GeoNodeLeaf* leaf = getLeaf(info->getHash);
            leaf->users.removeOne(info);

            _users.remove(info->session);
            it = _lru.erase(it);

            delete info;
        }
        else
        {
            break;
        }
    }
#endif

#if 0
    for(auto it = _users.begin(); it != _users.end();)
    {
        UserInfo* info = it.value();
        info->tickCounter++;
        if(info->tickCounter >= 6)
        {
            GeoNodeLeaf* leaf = getLeaf(info->getHash);
            leaf->users.removeOne(info);

            delete info;
            it = _users.erase(it);
        }
        else
        {
            ++it;
        }
    }
#endif
}


int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    for(uint32_t i=0 ;i<sizeof(base32encode); ++i)
    {
        base32encode[i] = 'A' + i;
    }

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
