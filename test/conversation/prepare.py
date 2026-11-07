#!/usr/bin/env python3
"""Build-only fixtures: actual SDDM parser/proxy and helper methods, no PAM/login."""
from pathlib import Path
import sys
source, target = map(Path, sys.argv[1:3]);target.mkdir(parents=True,exist_ok=True)
def method(text, marker):
    start=text.index(marker);end=text.index('{',start)+1;depth=1
    while depth:
        if text[end]=='{':depth+=1
        elif text[end]=='}':depth-=1
        end+=1
    return text[start:end]
# Keep the actual daemon socket parser; replace only unrelated power/platform deps.
socket=(source/'src/daemon/SocketServer.cpp').read_text()
for include in ['DaemonApp.h','PowerManager.h','Utils.h']:
    socket=socket.replace('#include "'+include+'"','#include "FakePlatform.h"')
(target/'SocketServer-fixture.cpp').write_text(socket)
helper=(source/'src/helper/HelperApp.cpp').read_text()
methods='\n'.join(method(helper, marker).replace('HelperApp::','HelperFixture::').replace('getuid()', 'fixtureHelperUid()') for marker in
                 ['void HelperApp::doAuth()', 'QProcessEnvironment HelperApp::authenticated('])
# The production doAuth/handshake methods are compiled unchanged except class name.
(target/'HelperMethods.inc').write_text(methods)
display=(source/'src/daemon/Display.cpp').read_text()
methods='\n'.join(method(display, marker).replace('Display::','DisplayFixture::') for marker in [
 'void Display::login(', 'void Display::beginAuthentication(', 'void Display::respondConversation(', 'void Display::cancelAuthentication(', 'void Display::finishConversation(',
 'void Display::slotRequestChanged()', 'void Display::slotAuthenticationFinished(', 'void Display::slotHelperFinished('])
methods=methods.replace('XorgDisplayServer','FixtureXorgDisplayServer').replace('OrgFreedesktopLogin1ManagerInterface','FixtureManager')
methods=methods.replace('Logind::','FixtureLogind::').replace('QDBusConnection::systemBus()','0')
methods=methods.replace('mainConfig.','fixtureMainConfig.').replace('stateConfig.','fixtureStateConfig.')
(target/'DisplayMethods.inc').write_text(methods)
auth=(source/'src/auth/Auth.cpp').read_text()
body=method(auth,'void Auth::Private::dataPending()').replace('Auth::Private::','AuthDispatchFixture::')
body=body.replace('Auth *auth = qobject_cast<Auth*>(parent());','FixtureAuthEvents *auth = events;')
body+='\n'+method(auth,'void Auth::Private::requestFinished()').replace('Auth::Private::','AuthDispatchFixture::')
(target/'AuthDispatch.inc').write_text(body)
pam=(source/'src/helper/backend/PamBackend.cpp').read_text()
(target/'PamConversation.inc').write_text(method(pam,'int PamBackend::converse(').replace('PamBackend::','PamConversationFixture::'))
(target/'PamAuthenticate.inc').write_text(method(pam,'bool PamBackend::authenticate()').replace('PamBackend::','FixtureBackend::'))

# Exercise the bundled theme's actual JavaScript without loading a login UI.
theme=(source/'src/greeter/theme/Main.qml').read_text()
functions=theme[theme.index('    function clearResponse()'):theme.index('    TextConstants {')]
(target/'ThemeMethods.inc').write_text('R"SDDMTHEME('+functions+')SDDMTHEME"')

(target/'HelperRequest.inc').write_text(method(helper,'Request HelperApp::request(').replace('HelperApp::','FrameHelper::'))
handshake=method(auth,'void Auth::SocketServer::handleNewConnection()').replace('Auth::SocketServer::','HandshakeServer::')
handshake=handshake.replace('qobject_cast<Auth *>(helper->parent())','helper->owner')
(target/'HandshakeMethods.inc').write_text(handshake)
start=method(auth,'void Auth::start()').replace('Auth::','StartFixture::').replace('SocketServer::instance()', 'FixtureRegistry::instance()').replace('Private::lastId', 'FixtureRegistry::lastId')
(target/'StartMethod.inc').write_text(start)
