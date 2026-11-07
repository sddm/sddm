#include <QtTest>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcessEnvironment>
#include <QUuid>
#include <QPointer>
#include <functional>
#include <security/pam_appl.h>
#include <pwd.h>
#include <unistd.h>
#include "Messages.h"
#include "GreeterProxy.h"
#include "SocketServer.h"
#include "Session.h"
#include "AuthMessages.h"
#include "FakePlatform.h"
namespace SDDM {FixtureDaemon fixtureDaemon;FixtureDaemon *daemonApp=&fixtureDaemon;}
using namespace SDDM;

// Fake only PAM/session/platform effects. Compile the actual helper branch and ACK check.
namespace SDDM {
inline uid_t fixtureHelperUid(){return 0;} // Model native root helper, never elevate the test process.
struct FixtureSocket {int frames=1,disconnects=0;void waitForBytesWritten(int=0){}int bytesAvailable()const{return frames;}void disconnectFromServer(){disconnects++;frames=0;}};
struct FixtureBackend;
struct FixturePamAuth {FixtureBackend*owner;bool authenticate();bool acctMgmt();QString errorString()const{return QStringLiteral("synthetic failure");}};
struct FixtureBackend {
 bool started=true,authenticated=true,accountAllowed=true;
 int authCalls=0,accountCalls=0,openCalls=0;
 bool start(const QString&){return started;}
 FixturePamAuth pam{this};FixturePamAuth*m_pam=&pam;FixtureBackend*m_app=this;
 void error(const QString&,Auth::Error){}bool authenticate();std::function<void()>onOpen;

 QString userName(){return QStringLiteral("canonical-fixture");}
 bool openSession(){openCalls++;if(onOpen)onOpen();return true;}
};
bool FixturePamAuth::authenticate(){owner->authCalls++;return owner->authenticated;}
bool FixturePamAuth::acctMgmt(){owner->accountCalls++;return owner->accountAllowed;}
#include "PamAuthenticate.inc"
struct FixtureSession {
 int starts=0;QString command=QStringLiteral("never-executed");
 QString path()const{return command;}QProcessEnvironment processEnvironment()const{return {};}
 void setProcessEnvironment(const QProcessEnvironment&){}bool start(){starts++;return true;}
 qint64 processId()const{return 1;}
};
struct FixtureWire {
 static inline Msg reply=AUTHENTICATED;static inline bool disconnected=false;
 static inline Msg sent=MSG_UNKNOWN;static inline int authAcks=0,sessionAcks=0,requestSends=0;
 static inline Request serialized;static inline std::function<void()>duringSend;
};
struct SafeDataStream {
 FixtureSocket*socket;explicit SafeDataStream(FixtureSocket*value):socket(value){}
 SafeDataStream&operator<<(Msg value){FixtureWire::sent=value;return *this;}
 SafeDataStream&operator<<(const Request&value){FixtureWire::serialized=value;return *this;}
 template<class T>SafeDataStream&operator<<(const T&){return *this;}
 SafeDataStream&operator>>(Msg& value){value=FixtureWire::reply;return *this;}
 SafeDataStream&operator>>(QString&value){value=QStringLiteral("canonical-fixture");return *this;}
 SafeDataStream&operator>>(bool&value){value=true;return *this;}
 template<class T>SafeDataStream&operator>>(T&){return *this;}
 void send(){if(FixtureWire::sent==REQUEST){FixtureWire::requestSends++;if(FixtureWire::duringSend)FixtureWire::duringSend();}if(FixtureWire::sent==AUTHENTICATED)FixtureWire::authAcks++;if(FixtureWire::sent==SESSION_STATUS)FixtureWire::sessionAcks++;}void receive(bool=false){socket->frames--;}void reset(){}
 QDataStream::Status status()const{return FixtureWire::disconnected?QDataStream::ReadPastEnd:QDataStream::Ok;}
};
class HelperFixture {
public:
 FixtureBackend backend;FixtureSession session;FixtureSocket socket;
 HelperFixture(){backend.onOpen=[this]{session.start();};}
 FixtureBackend*m_backend=&backend;FixtureSession*m_session=&session;FixtureSocket*m_socket=&socket;
 QString m_user=QStringLiteral("requested@example.test");qint64 m_id=1;bool m_conversation=false,m_sessionAuthorized=false;
 QByteArray m_cookie;int exitCode=-99;void exit(int code){exitCode=code;}
 void utmpLogin(const QString&,const QString&,const QString&,qint64,bool){}
 void sessionOpened(bool){}
 void doAuth();QProcessEnvironment authenticated(const QString&);
};
#include "HelperMethods.inc"
}

namespace SDDM {
static int fixtureUnlocks=0,fixtureActivations=0;
struct FixtureManager {FixtureManager(const QString&,const QString&,int){}void UnlockSession(const QString&){fixtureUnlocks++;}void ActivateSession(const QString&){fixtureActivations++;}};
struct FixtureLogind {static QString serviceName(){return {};}static QString managerPath(){return {};}};
class FixtureXorgDisplayServer:public QObject {Q_OBJECT public:QByteArray cookie()const{return {};}};
struct FixtureFlag {bool get()const{return false;}};
struct FixtureUsers {FixtureFlag RememberLastUser,RememberLastSession;};
static struct {FixtureUsers Users;} fixtureMainConfig;
struct FixtureSetting {void set(const QString&){}void setDefault(){}};
static struct {struct {FixtureSetting User,Session;} Last;int saves=0;void save(){saves++;}} fixtureStateConfig;
struct FixturePrompt {QByteArray response;void setResponse(const QByteArray&value){response=value;}bool hidden()const{return true;}QString message()const{return QStringLiteral("fresh prompt");}};
struct FixtureRequest {FixturePrompt prompt;int completions=0;QList<FixturePrompt*>prompts(){return {&prompt};}void done(){completions++;}};
struct FixtureAuth {
 bool active=true,cancelled=false,interactive=false;int starts=0;FixtureRequest req;std::function<void()>onStop;
 bool isActive()const{return active;}bool isCancelled()const{return cancelled;}bool conversation()const{return interactive;}bool autologin()const{return false;}
 void setAutologin(bool){}void setGreeter(bool){}void setSession(const QString&){}void setDisplayServerCommand(const QString&){}void setUser(const QString&){}void setConversation(bool value){interactive=value;}void start(){starts++;active=true;}
 void stop(){cancelled=true;if(onStop)onStop();active=false;}FixtureRequest*request(){return &req;}
 QString user()const{return QStringLiteral("canonical");}void setCookie(const QByteArray&){}
};
struct FixtureSink {int finishes=0;quint32 result=99,prompt=0;void conversationFinished(QLocalSocket*,quint32,quint32 value){finishes++;result=value;}
 void conversationAuthorized(QLocalSocket*,quint32){}void conversationPrompt(QLocalSocket*,quint32,quint32 value,const QString&,bool){prompt=value;}};
class DisplayFixture:public QObject {
public:
 FixtureAuth auth;FixtureAuth*m_auth=&auth;FixtureSink sink;FixtureSink*m_socketServer=&sink;
 quint32 m_conversationId=2,m_promptId=4;bool m_waitingPrompt=true,m_cancelledConversation=false,m_sessionAuthorized=false,
 m_started=false,m_legacyInitialRequest=false;
 QPointer<QLocalSocket>m_conversationSocket,m_socket;QString m_passPhrase,m_reuseSessionId,m_sessionName;QObject*m_displayServer=nullptr;
 QHash<QLocalSocket*,quint32>m_seenAttempts;int helperStarts=0;
 bool startAuth(const QString&,const QString&,const Session&){helperStarts++;return false;}
 int loginSuccess=0,loginFailure=0,stops=0;
 void loginSucceeded(QLocalSocket*){loginSuccess++;}void loginFailed(QLocalSocket*){loginFailure++;}void stop(){stops++;}void handleAutologinFailure(){}
 void login(QLocalSocket*,const QString&,const QString&,const Session&);
 void beginAuthentication(QLocalSocket*,quint32,const QString&,const Session&);
 void respondConversation(QLocalSocket*,quint32,quint32,const QString&);void cancelAuthentication(QLocalSocket*,quint32);
 void finishConversation(quint32);void slotRequestChanged();void slotAuthenticationFinished(const QString&,bool);void slotHelperFinished(Auth::HelperExitStatus);
};
#include "DisplayMethods.inc"
}

namespace SDDM {
struct FixtureAuthEvents {
 int authentications=0,sessions=0;std::function<void()>duringAuthentication;
 void setUser(const QString&){}void authentication(const QString&,bool){authentications++;if(duringAuthentication)duringAuthentication();}
void sessionStarted(bool){sessions++;}void displayServerReady(const QString&){}
 void error(const QString&,Auth::Error){}void info(const QString&,Auth::Info){}
};
struct FixtureAuthRequest {Request current;Request request()const{return current;}void setRequest(const Request*value=nullptr){current=value?*value:Request();}};
class AuthDispatchFixture {
public:
 using Error=Auth::Error;using Info=Auth::Info;
 static constexpr auto ERROR_NONE=Auth::ERROR_NONE,ERROR_INTERNAL=Auth::ERROR_INTERNAL;
 static constexpr auto INFO_NONE=Auth::INFO_NONE;
 FixtureSocket store;FixtureSocket*socket=&store;FixtureAuthEvents eventStore;FixtureAuthEvents*events=&eventStore;
 FixtureAuthRequest requestStore;FixtureAuthRequest*request=&requestStore;
 bool cancelled=false,authResultSeen=false,authenticationSucceeded=false;
 QProcessEnvironment environment;QByteArray cookie;QString sessionPath,displayServerCmd;
 void dataPending();void requestFinished();
};
#include "AuthDispatch.inc"
struct FixturePamApp {
 QStringList infos,errors,prompts;QList<bool>hidden;QList<QByteArray>answers;
 void info(const QString&text,Auth::Info){infos<<text;}void error(const QString&text,Auth::Error){errors<<text;}
 Request request(const Request&sent){prompts<<sent.prompts[0].message;hidden<<sent.prompts[0].hidden;Request reply(sent);if(answers.isEmpty())return {};reply.prompts[0].response=answers.takeFirst();return reply;}
};
class PamConversationFixture {public:FixturePamApp app;FixturePamApp*m_app=&app;int converse(int,const pam_message**,pam_response**);};
#include "PamConversation.inc"
}

class NativeProofs:public QObject {
 Q_OBJECT
private slots:
 void initTestCase(){qRegisterMetaType<QLocalSocket*>();}
 void fragmentedInitializationAndPrompts(){
  QLocalServer server;QVERIFY(server.listen(QStringLiteral("sddm-proxy-proof-")+QUuid::createUuid().toString(QUuid::WithoutBraces)));
  GreeterProxy proxy(server.fullServerName());QSignalSpy prompts(&proxy,&GreeterProxy::authenticationPrompt);
  QTRY_VERIFY(server.hasPendingConnections());auto peer=server.nextPendingConnection();
  QTRY_VERIFY(peer->bytesAvailable()>=4);peer->readAll();
  QByteArray frame;QDataStream out(&frame,QIODevice::WriteOnly);
  out<<quint32(DaemonMessages::Capabilities)<<quint32(3)
     <<quint32(DaemonMessages::HostName)<<QStringLiteral("fragmented-host")
     <<quint32(DaemonMessages::ConversationVersion)<<quint32(1)
     <<quint32(DaemonMessages::ConversationPrompt)<<quint32(7)<<quint32(4)<<QStringLiteral("Number 42, then OTP:")<<quint32(0);
  for(int i=0;i<frame.size()-1;i++){peer->write(frame.mid(i,1));peer->flush();QTest::qWait(1);QCOMPARE(prompts.count(),0);}
  peer->write(frame.right(1));peer->flush();QTRY_COMPARE(prompts.count(),1);
  QCOMPARE(proxy.hostName(),QStringLiteral("fragmented-host"));QCOMPARE(proxy.authenticationConversationVersion(),quint32(1));
  QCOMPARE(prompts.at(0).at(0).toUInt(),quint32(7));QCOMPARE(prompts.at(0).at(1).toUInt(),quint32(4));
  QCOMPARE(prompts.at(0).at(3).toBool(),true);
  proxy.respond(7,4,QStringLiteral("synthetic-response"));
  QTRY_VERIFY(peer->bytesAvailable()>0);QDataStream in(peer);quint32 opcode=0,attempt=0,prompt=0;QString response;
  in>>opcode>>attempt>>prompt>>response;QCOMPARE(opcode,quint32(GreeterMessages::ConversationResponse));QCOMPARE(attempt,quint32(7));QCOMPARE(prompt,quint32(4));QCOMPARE(response,QStringLiteral("synthetic-response"));
 }
 void legacyConnectPreservesWireAndConversationIsExplicit(){
  SocketServer server;QVERIFY(server.start(QStringLiteral("legacy-connect")));QLocalSocket client;
  client.connectToServer(server.socketAddress());QVERIFY(client.waitForConnected());
  QByteArray frame;QDataStream out(&frame,QIODevice::WriteOnly);out<<quint32(GreeterMessages::Connect);
  client.write(frame);client.flush();QTRY_VERIFY(client.bytesAvailable()>0);
  QDataStream input(&client);quint32 code=99,caps=0;QString hostname;
  input>>code>>caps;QCOMPARE(code,quint32(DaemonMessages::Capabilities));
  input>>code>>hostname;QCOMPARE(code,quint32(DaemonMessages::HostName));QCOMPARE(hostname,QStringLiteral("fixture-host"));
  QVERIFY(client.bytesAvailable()==0);
  QByteArray hello;QDataStream helloOut(&hello,QIODevice::WriteOnly);helloOut<<quint32(GreeterMessages::ConversationHello);
  client.write(hello);client.flush();QTRY_VERIFY(client.bytesAvailable()>0);
  input>>code>>caps;QCOMPARE(code,quint32(DaemonMessages::ConversationVersion));QCOMPARE(caps,quint32(1));
 }
 void fragmentedDaemonResponseAndBegin(){
  SocketServer server;QVERIFY(server.start(QStringLiteral("proof")));QLocalSocket client;client.connectToServer(server.socketAddress());QVERIFY(client.waitForConnected());
  QSignalSpy responses(&server,&SocketServer::conversationResponse),begin(&server,&SocketServer::conversationBegin),cancel(&server,&SocketServer::conversationCancel);
  QByteArray frame;QDataStream out(&frame,QIODevice::WriteOnly);
  out<<quint32(GreeterMessages::BeginConversation)<<quint32(9)<<QStringLiteral("fixture@example.test")<<Session()
     <<quint32(GreeterMessages::ConversationResponse)<<quint32(9)<<quint32(3)<<QStringLiteral("fresh-only")
     <<quint32(GreeterMessages::CancelConversation)<<quint32(9);
  for(int i=0;i<frame.size();i++){client.write(frame.mid(i,1));client.flush();QTest::qWait(1);}
  QTRY_COMPARE(begin.count(),1);QTRY_COMPARE(responses.count(),1);QTRY_COMPARE(cancel.count(),1);
  QCOMPARE(responses.at(0).at(1).toUInt(),quint32(9));QCOMPARE(responses.at(0).at(2).toUInt(),quint32(3));QCOMPARE(responses.at(0).at(3).toString(),QStringLiteral("fresh-only"));
 }
 void optionLookingUsersNeverLaunchHelpers(){
  for(const auto&user:{QStringLiteral("--conversation"),QStringLiteral("--autologin"),QStringLiteral("--greeter")}){
   QLocalSocket owner;DisplayFixture legacy;legacy.auth.active=false;legacy.m_conversationId=0;legacy.login(&owner,user,QStringLiteral("unused"),Session());QCOMPARE(legacy.helperStarts,0);QCOMPARE(legacy.auth.starts,0);
   DisplayFixture native;native.auth.active=false;native.m_conversationId=0;native.beginAuthentication(&owner,1,user,Session());QCOMPARE(native.helperStarts,0);QCOMPARE(native.auth.starts,0);
  }
 }
 void staleDuplicateResponsesAndCancelledSuccess(){
  QLocalSocket owner,other;DisplayFixture display;display.m_conversationSocket=&owner;display.m_socket=&owner;
  display.respondConversation(&owner,1,4,QStringLiteral("old-attempt"));display.respondConversation(&owner,2,3,QStringLiteral("old-prompt"));display.respondConversation(&other,2,4,QStringLiteral("other-socket"));QCOMPARE(display.auth.req.completions,0);
  display.respondConversation(&owner,2,4,QStringLiteral("fresh"));display.respondConversation(&owner,2,4,QStringLiteral("duplicate"));QCOMPARE(display.auth.req.completions,1);
  display.slotRequestChanged();QCOMPARE(display.m_promptId,quint32(5));display.respondConversation(&owner,2,4,QStringLiteral("late"));QCOMPARE(display.auth.req.completions,1);
  display.respondConversation(&owner,2,5,QStringLiteral("new-fresh"));QCOMPARE(display.auth.req.completions,2);
  fixtureUnlocks=fixtureActivations=fixtureStateConfig.saves=0;display.m_reuseSessionId=QStringLiteral("existing-session");
  display.auth.onStop=[&]{display.slotAuthenticationFinished(QStringLiteral("canonical"),true);};
  display.cancelAuthentication(&owner,2);QCOMPARE(display.loginSuccess,0);QCOMPARE(fixtureUnlocks,0);QCOMPARE(fixtureActivations,0);QCOMPARE(fixtureStateConfig.saves,0);
 }

 void legacyPasswordIsConsumedOnceAndAdditionalPromptFails(){
  QLocalSocket owner; DisplayFixture display; display.m_conversationId=0; display.m_socket=&owner;
  display.m_legacyInitialRequest=true; display.m_passPhrase=QStringLiteral("first-only");
  display.slotRequestChanged();QCOMPARE(display.auth.req.completions,1);
  QCOMPARE(display.auth.req.prompt.response,QByteArray("first-only"));QVERIFY(display.m_passPhrase.isEmpty());
  display.slotRequestChanged();QCOMPARE(display.auth.req.completions,1);QCOMPARE(display.loginFailure,1);QVERIFY(display.auth.cancelled);
 }
 void reusedSessionProducesConversationSuccess(){
  QLocalSocket owner;DisplayFixture reused;reused.m_conversationSocket=&owner;reused.m_sessionAuthorized=true;
  reused.slotHelperFinished(Auth::HELPER_SUCCESS);QCOMPARE(reused.sink.result,quint32(ConversationLogin));
 }
 void actualAuthDispatcherGatesCancellationAndRoles(){
  auto frame=[](AuthDispatchFixture&dispatcher,Msg opcode){FixtureWire::reply=opcode;FixtureWire::disconnected=false;FixtureWire::authAcks=FixtureWire::sessionAcks=0;dispatcher.store.frames=1;dispatcher.dataPending();};
  AuthDispatchFixture cancelled;cancelled.cancelled=true;frame(cancelled,AUTHENTICATED);QCOMPARE(cancelled.events->authentications,0);QCOMPARE(FixtureWire::authAcks,0);QCOMPARE(cancelled.store.disconnects,1);
  AuthDispatchFixture normal;frame(normal,AUTHENTICATED);QCOMPARE(normal.events->authentications,1);QCOMPARE(FixtureWire::authAcks,1);QVERIFY(normal.authenticationSucceeded);
  AuthDispatchFixture cancelledDuringAck;cancelledDuringAck.events->duringAuthentication=[&]{cancelledDuringAck.cancelled=true;};frame(cancelledDuringAck,AUTHENTICATED);QCOMPARE(cancelledDuringAck.events->authentications,1);QCOMPARE(FixtureWire::authAcks,0);
 }
 void requestFinishedPreservesReentrantNextRequest(){
  AuthDispatchFixture dispatcher;Request first,second;
  Prompt response;response.response=QByteArray("first-response");first.prompts.append(response);
  Prompt next;next.message=QStringLiteral("second prompt");second.prompts.append(next);
  dispatcher.request->setRequest(&first);FixtureWire::requestSends=0;
  FixtureWire::duringSend=[&]{dispatcher.request->setRequest(&second);};
  dispatcher.requestFinished();FixtureWire::duringSend={};
  QCOMPARE(FixtureWire::requestSends,1);QCOMPARE(FixtureWire::serialized.prompts.size(),1);
  QCOMPARE(FixtureWire::serialized.prompts[0].response,QByteArray("first-response"));
  QCOMPARE(dispatcher.request->request().prompts.size(),1);
  QCOMPARE(dispatcher.request->request().prompts[0].message,QStringLiteral("second prompt"));
  dispatcher.cancelled=true;dispatcher.requestFinished();QCOMPARE(FixtureWire::requestSends,1);
  dispatcher.cancelled=false;dispatcher.socket=nullptr;dispatcher.requestFinished();QCOMPARE(FixtureWire::requestSends,1);
 }
 void actualPamConversationUsesFreshResponsesForEachMessage(){
  PamConversationFixture backend;backend.app.answers={QByteArray("first-pin"),QByteArray("visible-otp"),QByteArray("fresh-retry")};
  pam_message info{PAM_TEXT_INFO,"Use number 42"},hidden{PAM_PROMPT_ECHO_OFF,"PIN:"},visible{PAM_PROMPT_ECHO_ON,"OTP:"},error{PAM_ERROR_MSG,"Try again"};
  const pam_message*batch[]={&info,&hidden,&visible,&error};pam_response*answers=nullptr;
  QCOMPARE(backend.converse(4,batch,&answers),PAM_SUCCESS);QCOMPARE(backend.app.prompts.size(),2);QCOMPARE(backend.app.hidden.at(0),true);QCOMPARE(backend.app.hidden.at(1),false);
  QCOMPARE(QByteArray(answers[1].resp),QByteArray("first-pin"));QCOMPARE(QByteArray(answers[2].resp),QByteArray("visible-otp"));
  for(int i=0;i<4;i++)free(answers[i].resp);free(answers);
  const pam_message*retry[]={&hidden};QCOMPARE(backend.converse(1,retry,&answers),PAM_SUCCESS);QCOMPARE(QByteArray(answers[0].resp),QByteArray("fresh-retry"));free(answers[0].resp);free(answers);
  const pam_message*notice[]={&info};QCOMPARE(backend.converse(1,notice,&answers),PAM_SUCCESS);QCOMPARE(backend.app.prompts.size(),3);free(answers);
 }
 void missingOrWrongAckNeverOpensSession(){
  for(int variant=0;variant<3;variant++){
   FixtureWire::disconnected=variant==0;FixtureWire::reply=variant==1?HELLO_CONVERSATION:AUTHENTICATED;
   HelperFixture helper;helper.doAuth();
   QCOMPARE(helper.backend.authCalls,1);QCOMPARE(helper.backend.accountCalls,1);
   QCOMPARE(helper.backend.openCalls,variant==2?1:0);QCOMPARE(helper.session.starts,variant==2?1:0);
  }
 }
 void accountDenialNeverOpensSession(){
  FixtureWire::disconnected=false;FixtureWire::reply=AUTHENTICATED;
  HelperFixture helper;helper.backend.accountAllowed=false;helper.doAuth();QCOMPARE(helper.backend.accountCalls,1);QCOMPARE(helper.backend.openCalls,0);QCOMPARE(helper.session.starts,0);
 }

};
QTEST_GUILESS_MAIN(NativeProofs)
#include "proofs.moc"
