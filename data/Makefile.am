## Process this file with automake to produce Makefile.in

if CLIENT
CLIENT_FILES =				\
	amplio2.tilespec		\
	cimpletoon.tilespec		\
	hex2t.tilespec			\
	hexemplio.tilespec		\
	isophex.tilespec		\
	isotrident.tilespec		\
	toonhex.tilespec		\
	trident.tilespec		\
	stdmusic.musicspec		\
	stdsounds.soundspec		\
	helpdata.txt
if CLIENT_GUI_XAW
CLIENT_FILES += Freeciv
endif
if CLIENT_GUI_GTK_2_0
CLIENT_FILES += \
	freeciv.rc-2.0	\
	gtk2_menus.xml
endif
if CLIENT_GUI_GTK_3_0
CLIENT_FILES += \
	gtk3_menus.xml
endif
if CLIENT_GUI_GTK_3_22
CLIENT_FILES += \
	gtk3.22_menus.xml
endif
else
CLIENT_FILES =
endif

SRV_RE_FILES = \
	civ1.serv \
	civ2.serv \
	classic.serv \
	default.serv \
	experimental.serv \
	multiplayer.serv \
	civ2civ3.serv \
	sandbox.serv

if SERVER
SERVER_FILES = $(SRV_RE_FILES)
else
if RULEDIT
SERVER_FILES = $(SRV_RE_FILES)
else
SERVER_FILES =
endif
endif

pkgdata_DATA = $(SERVER_FILES) $(CLIENT_FILES)

EXTRA_DIST = \
	Freeciv	\
	freeciv-client.png \
	freeciv.rc-2.0	\
	gtk2_menus.xml	\
	gtk3.22_menus.xml \
	gtk3_menus.xml	\
	amplio2.tilespec \
	cimpletoon.tilespec \
	hex2t.tilespec	\
	hexemplio.tilespec	\
	isophex.tilespec	\
	isotrident.tilespec \
	toonhex.tilespec	\
	trident.tilespec	\
	stdmusic.musicspec	\
	stdsounds.soundspec	\
	helpdata.txt	\
	civ1.serv	\
	civ2.serv	\
	classic.serv	\
	default.serv	\
	experimental.serv \
	multiplayer.serv \
	civ2civ3.serv \
	sandbox.serv

if CLIENT
CLIENTDATADIRS = amplio2 misc flags buildings cimpletoon isotrident trident\
 isophex hex2t hexemplio stdsounds stdmusic themes wonders
else
CLIENTDATADIRS = 
endif

SRV_RE_DATADIRS = default classic civ1 civ2 experimental multiplayer civ2civ3 sandbox nation
if SERVER
SERVERDATADIRS = $(SRV_RE_DATADIRS) scenarios
else
if RULEDIT
SERVERDATADIRS = $(SRV_RE_DATADIRS)
else
SERVERDATADIRS =
endif
endif

if CLIENT
ICONS = freeciv-client.png
else
ICONS = 
endif

icondir = $(prefix)/share/pixmaps
icon_DATA=$(ICONS)

SUBDIRS = icons $(CLIENTDATADIRS) $(SERVERDATADIRS) override
