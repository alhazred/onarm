// Copyright (c) 1995 James Clark
// See the file COPYING for copying permission.
#pragma ident	"@(#)events.h	1.4	00/07/17 SMI"

EVENT(MessageEvent, message)
EVENT(DataEvent, data)
EVENT(StartElementEvent, startElement)
EVENT(EndElementEvent, endElement)
EVENT(PiEvent, pi)
EVENT(SdataEntityEvent, sdataEntity)
EVENT(ExternalDataEntityEvent, externalDataEntity)
EVENT(SubdocEntityEvent, subdocEntity)
EVENT(NonSgmlCharEvent, nonSgmlChar)
EVENT(AppinfoEvent, appinfo)
EVENT(UselinkEvent, uselink)
EVENT(UsemapEvent, usemap)
EVENT(StartDtdEvent, startDtd)
EVENT(EndDtdEvent, endDtd)
EVENT(StartLpdEvent, startLpd)
EVENT(EndLpdEvent, endLpd)
EVENT(EndPrologEvent, endProlog)
EVENT(SgmlDeclEvent, sgmlDecl)
EVENT(CommentDeclEvent, commentDecl)
EVENT(SSepEvent, sSep)
EVENT(IgnoredReEvent, ignoredRe)
EVENT(ReOriginEvent, reOrigin)
EVENT(IgnoredRsEvent, ignoredRs)
EVENT(IgnoredCharsEvent, ignoredChars)
EVENT(MarkedSectionStartEvent, markedSectionStart)
EVENT(MarkedSectionEndEvent, markedSectionEnd)
EVENT(EntityStartEvent, entityStart)
EVENT(EntityEndEvent, entityEnd)
EVENT(EntityDeclEvent, entityDecl)
EVENT(NotationDeclEvent, notationDecl)
EVENT(ElementDeclEvent, elementDecl)
EVENT(AttlistDeclEvent, attlistDecl)
EVENT(LinkAttlistDeclEvent, linkAttlistDecl)
EVENT(AttlistNotationDeclEvent, attlistNotationDecl)
EVENT(LinkDeclEvent, linkDecl)
EVENT(IdLinkDeclEvent, idLinkDecl)
EVENT(ShortrefDeclEvent, shortrefDecl)
EVENT(IgnoredMarkupEvent, ignoredMarkup)
EVENT(EntityDefaultedEvent, entityDefaulted)
EVENT(SgmlDeclEntityEvent, sgmlDeclEntity)
