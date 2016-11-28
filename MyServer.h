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
#include <QMap>

using namespace Tufao;

class HCListNode
{
public:
    HCListNode* next;
    HCListNode* prev;

    HCListNode()
    {
        next = prev = this;
    }
};

class HCList
{
public:
    HCListNode head;

    HCList()
    {
        head.next = head.prev = &head;
    }

    void addNode(HCListNode* prev, HCListNode* next, HCListNode* node)
    {
        prev->next = node;
        next->prev = node;
        node->next = next;
        node->prev = prev;
    }

    void addTail(HCListNode* node)
    {
        addNode(&head, head.next, node);
    }

    void delNode(HCListNode *prev, HCListNode* next)
    {
        prev->next = next;
        next->prev = prev;
    }

    void delNode(HCListNode* node)
    {
        delNode(node->prev, node->next);
    }
};

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

#define HC_USER_IDLE 0
#define HC_USER_BUSY 1
#define HC_USER_PENDING 2 // 有订单，但是司机还不知道
class UserInfo
{
public:
    // 一定放在第一个，为了方便
    HCListNode node;
    QString username;
    double lat;
    double lng;
    QString type;
    QString session;
    quint64 timestamp; // 时间点 23:00:59 24:00:00 记录最近访问的时间点
    QByteArray getHash; // vf2g
    int status;

    UserInfo()
    {
        status = HC_USER_IDLE;
        lat = lng = 0;
    }
};

class GeoNode
{
public:
    GeoNode* child[32];
    GeoNode* parent;

    GeoNode()
    {
        memset(child, 0, sizeof(child));
        parent = NULL;
    }
};

class GeoNodeLeaf : public GeoNode
{
public:
    QList<UserInfo*> users;
};

class MyServer : public QObject
{
    Q_OBJECT
public:
    explicit MyServer(QObject *parent = 0);

    Tufao::HttpServer* _server;

    void handle(HttpServerRequest& req, HttpServerResponse& rep);

    QJsonObject handleQuery(QJsonObject obj);
    QJsonObject handleInsert(QJsonObject obj);
    QJsonObject handleInsertP(QJsonObject obj);
    QJsonObject handleInsertT(QJsonObject obj);
    QJsonObject handleUpdate(QJsonObject obj);
    QJsonObject handleGet(QJsonObject obj);

    QByteArray geohashAdd(QByteArray geohash, int v);

    QMap<QString, UserInfo*> _users; // O(log(n)) 红黑树
    GeoNode _head; // 8次 O(1)
    // 保存那些是经常用的，那些是不经常使用的，经常使用的在尾部
 //   QList<UserInfo*> _lru;
   //ListNode _lru;

    HCList _lru; // O(1)

    GeoNodeLeaf* getLeaf(QByteArray geohash);

    void timerEvent(QTimerEvent *ev);
signals:

public slots:
};

#endif // MYSERVER_H
