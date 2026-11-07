#include <QtTest>
#include <QJSEngine>

class ConversationThemeTest : public QObject {
    Q_OBJECT
private slots:
    void freshResponsesCancellationAndRetry() {
        QJSEngine engine;
        const QString fixture = QStringLiteral(R"JS(
var conversationAvailable=true, activeAttempt=0, activePrompt=0;
var awaitingResponse=false, responseVisible=false, cancelling=false, authorized=false;
var promptText='', notices=[], conversationStatus='', sessionIndex=0;
var response={text:'', forceActiveFocus:function(){}}, username={text:'alice'};
var textConstants={loginFailed:'failed'}, replies=[], cancelled=[], starts=0;
function qsTr(text){return text;}
var sddm={beginAuthentication:function(){return ++starts;},
 respond:function(attempt,prompt,value){replies.push([attempt,prompt,value]);},
 cancelAuthentication:function(attempt){cancelled.push(attempt);}};
function check(value){if(!value)throw new Error('theme assertion');}
)JS") + QString::fromUtf8(
#include "ThemeMethods.inc"
        ) + QStringLiteral(R"JS(
startAuthentication(); check(activeAttempt===1);
receivePrompt(1,1,'Password:',false); response.text='FAKE_PASSWORD'; sendResponse();
check(response.text==='' && !awaitingResponse && replies.length===1);
sendResponse(); check(replies.length===1);
receiveMessage(1,'Approve request number 42',false);
receivePrompt(1,2,'One-time code:',true);
check(responseVisible && response.text==='' && notices.length===1);
response.text='FAKE_OTP'; sendResponse(); check(replies[1][1]===2);
receivePrompt(0,3,'stale',false); check(!awaitingResponse);
receivePrompt(1,3,'Retry:',false); response.text='DISCARDED'; cancelAuthentication();
check(cancelling && response.text==='' && cancelled.length===1);
startAuthentication(); check(starts===1);
receivePrompt(1,4,'late',false); check(!awaitingResponse);
authenticationFinished(1,1); check(!authorized && activeAttempt===0);
startAuthentication(); check(activeAttempt===2);
authenticationFinished(1,2); check(activeAttempt===2 && !authorized);
receivePrompt(2,1,'Password:',false); response.text='FAKE_NEW'; sendResponse();
authenticationFinished(2,0); check(!authorized && response.text==='');
startAuthentication(); authenticationFinished(3,2); check(authorized);
startAuthentication(); check(starts===3);
)JS");
        const auto result=engine.evaluate(fixture);
        QVERIFY2(!result.isError(), qPrintable(result.toString()));
    }
};
QTEST_GUILESS_MAIN(ConversationThemeTest)
#include "theme-test.moc"
