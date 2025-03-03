/* Copyright 2021 Jaakko Keränen <jaakko.keranen@iki.fi>

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "uploadwidget.h"
#include "labelwidget.h"
#include "inputwidget.h"
#include "documentwidget.h"
#include "root.h"
#include "color.h"
#include "command.h"
#include "gmrequest.h"
#include "sitespec.h"
#include "window.h"
#include "gmcerts.h"
#include "app.h"

#if defined (iPlatformAppleMobile)
#   include "ios.h"
#endif

#include <the_Foundation/file.h>
#include <the_Foundation/fileinfo.h>
#include <the_Foundation/path.h>

iDefineObjectConstruction(UploadWidget)
    
enum iUploadIdentity {
    none_UploadIdentity,
    defaultForSite_UploadIdentity,
    dropdown_UploadIdentity,
};

struct Impl_UploadWidget {
    iWidget          widget;
    iString          originalUrl;
    iString          url;
    iDocumentWidget *viewer;
    iGmRequest *     request;
    iLabelWidget *   info;
    iInputWidget *   path;
    iInputWidget *   mime;
    iInputWidget *   token;
    iLabelWidget *   ident;       
    iInputWidget *   input;
    iLabelWidget *   filePathLabel;
    iLabelWidget *   fileSizeLabel;
    iLabelWidget *   counter;
    iString          filePath;
    size_t           fileSize;
    enum iUploadIdentity idMode;
    iBlock           idFingerprint;    
    iAtomicInt       isRequestUpdated;
};

static void releaseFile_UploadWidget_(iUploadWidget *d) {
#if defined (iPlatformAppleMobile)
    if (!isEmpty_String(&d->filePath)) {
        /* Delete the temporary file that was copied for uploading. */
        remove(cstr_String(&d->filePath));
    }
#endif
    clear_String(&d->filePath);
}

static void updateProgress_UploadWidget_(iGmRequest *request, size_t current, size_t total) {
    iUploadWidget *d = userData_Object(request);
    postCommand_Widget(d,
                       "upload.request.updated reqid:%u arg:%zu total:%zu",
                       id_GmRequest(request),
                       current,
                       total);
}

static void updateInputMaxHeight_UploadWidget_(iUploadWidget *d) {
    iWidget *w = as_Widget(d);    
    /* Calculate how many lines fits vertically in the view. */
    const iInt2 inputPos = topLeft_Rect(bounds_Widget(as_Widget(d->input)));
    int footerHeight = 0;
    if (!isUsingPanelLayout_Mobile()) {
        footerHeight = (height_Widget(d->token) +
                        height_Widget(findChild_Widget(w, "dialogbuttons")) +
                        12 * gap_UI);
    }
    const int avail = bottom_Rect(visibleRect_Root(w->root)) - footerHeight - inputPos.y;
    /* On desktop, retain the previously set minLines value. */
    int minLines = isUsingPanelLayout_Mobile() ? 1 : minLines_InputWidget(d->input);
    int maxLines = iMaxi(minLines, avail / lineHeight_Text(font_InputWidget(d->input)));
    /* On mobile, the height is fixed to the available space. */
    setLineLimits_InputWidget(d->input, isUsingPanelLayout_Mobile() ? maxLines : minLines, maxLines);
}

static const iGmIdentity *titanIdentityForUrl_(const iString *url) {
    return findIdentity_GmCerts(
        certs_App(),
        collect_Block(hexDecode_Rangecc(range_String(valueString_SiteSpec(
            collectNewRange_String(urlRoot_String(url)), titanIdentity_SiteSpecKey)))));
}

static const iArray *makeIdentityItems_UploadWidget_(const iUploadWidget *d) {
    iArray *items = collectNew_Array(sizeof(iMenuItem));
    const iGmIdentity *urlId = titanIdentityForUrl_(&d->url);
    pushBack_Array(items,
                   &(iMenuItem){ format_CStr("${dlg.upload.id.default} (%s)",
                                             urlId ? cstr_String(name_GmIdentity(urlId))
                                                   : "${dlg.upload.id.none}"),
                                 0, 0, "upload.setid arg:1" });
    pushBack_Array(items, &(iMenuItem){ "${dlg.upload.id.none}", 0, 0, "upload.setid arg:0" });
    pushBack_Array(items, &(iMenuItem){ "---" });
    iConstForEach(PtrArray, i, listIdentities_GmCerts(certs_App(), NULL, NULL)) {
        const iGmIdentity *id = i.ptr;
        iString *str = collect_String(copy_String(name_GmIdentity(id)));
        prependCStr_String(str, "\x1b[1m");
        if (!isEmpty_String(&id->notes)) {
            appendFormat_String(
                str, "\x1b[0m\n%s%s", escape_Color(uiTextDim_ColorId), cstr_String(&id->notes));
        }
        pushBack_Array(
            items,
            &(iMenuItem){ cstr_String(str), 0, 0,
                          format_CStr("upload.setid fp:%s",
                                      cstrCollect_String(hexEncode_Block(&id->fingerprint))) });
    }
    pushBack_Array(items, &(iMenuItem){ NULL });
    return items;
}

static void enableUploadButton_UploadWidget_(iUploadWidget *d, iBool enable) {
    if (isUsingPanelLayout_Mobile()) {
        iWidget *back = findChild_Widget(as_Widget(d), "panel.back");
        setFlags_Widget(child_Widget(back, 0), hidden_WidgetFlag, !enable);
        refresh_Widget(back);
    }
    else {
        /* Not on used in the desktop layout. */
    }
}

void init_UploadWidget(iUploadWidget *d) {
    iWidget *w = as_Widget(d);
    init_Widget(w);
    setId_Widget(w, "upload");
    init_String(&d->originalUrl);
    init_String(&d->url);
    d->viewer = NULL;
    d->request = NULL;
    init_String(&d->filePath);
    d->fileSize = 0;
    d->idMode = defaultForSite_UploadIdentity;
    init_Block(&d->idFingerprint, 0);
    const iMenuItem actions[] = {
        { "${upload.port}", 0, 0, "upload.setport" },
        { "---" },
        { "${close}", SDLK_ESCAPE, 0, "upload.cancel" },
        { uiTextAction_ColorEscape "${dlg.upload.send}", SDLK_RETURN, KMOD_PRIMARY, "upload.accept" }
    };
    if (isUsingPanelLayout_Mobile()) {
        const iMenuItem textItems[] = {
            { "title id:heading.upload.text" },
            { "input id:upload.text noheading:1" },
            { NULL }        
        };
        const iMenuItem fileItems[] = {
            { "title id:heading.upload.file" },
            { "button text:" uiTextAction_ColorEscape "${dlg.upload.pickfile}", 0, 0, "upload.pickfile" },            
            { "heading id:upload.file.name" },
            { "label id:upload.filepathlabel text:\u2014" },
            { "heading id:upload.file.size" },
            { "label id:upload.filesizelabel text:\u2014" },
            { "padding" },
            { "input id:upload.mime" },
            { "label id:upload.counter text:" },
            { NULL }        
        };
        initPanels_Mobile(w, NULL, (iMenuItem[]){
            { "title id:heading.upload" },
            { "heading id:upload.url" },
            { format_CStr("label id:upload.info font:%d",
                          deviceType_App() == phone_AppDeviceType ? uiLabelBig_FontId : uiLabelMedium_FontId) },
            { "input id:upload.path hint:hint.upload.path noheading:1 url:1 text:" },
            { "heading text:${heading.upload.id}" },
            { "dropdown id:upload.id icon:0x1f464 text:", 0, 0, constData_Array(makeIdentityItems_UploadWidget_(d)) },
            { "input id:upload.token hint:hint.upload.token.long icon:0x1f516 text:" },
            { "heading id:upload.content" },
            { "panel id:dlg.upload.text icon:0x1f5b9 noscroll:1", 0, 0, (const void *) textItems },
            { "panel id:dlg.upload.file icon:0x1f4c1", 0, 0, (const void *) fileItems },
            { NULL }
        }, actions, iElemCount(actions));
        d->info          = findChild_Widget(w, "upload.info");
        d->path          = findChild_Widget(w, "upload.path");
        d->input         = findChild_Widget(w, "upload.text");
        d->filePathLabel = findChild_Widget(w, "upload.filepathlabel");
        d->fileSizeLabel = findChild_Widget(w, "upload.filesizelabel");
        d->mime          = findChild_Widget(w, "upload.mime");
        d->token         = findChild_Widget(w, "upload.token");
        d->counter       = findChild_Widget(w, "upload.counter");
        if (isPortraitPhone_App()) {
            enableUploadButton_UploadWidget_(d, iFalse);
        }
        iWidget *title = findChild_Widget(w, "heading.upload.text");
        iLabelWidget *menu = new_LabelWidget(midEllipsis_Icon, "upload.editmenu.open");
        setTextColor_LabelWidget(menu, uiTextAction_ColorId);
        setFont_LabelWidget(menu, uiLabelBigBold_FontId);
        addChildFlags_Widget(title, iClob(menu), frameless_WidgetFlag | moveToParentRightEdge_WidgetFlag);
    }
    else {
        useSheetStyle_Widget(w);
        setFlags_Widget(w, overflowScrollable_WidgetFlag, iFalse);
        addDialogTitle_Widget(w, "${heading.upload}", NULL);
        iWidget *headings, *values;
        /* URL path. */ {
            iWidget *page = makeTwoColumns_Widget(&headings, &values);
            d->path = new_InputWidget(0);
            addTwoColumnDialogInputField_Widget(
                headings, values, "", "upload.path", iClob(d->path));
            d->info = (iLabelWidget *) lastChild_Widget(headings);
            setFont_LabelWidget(d->info, uiContent_FontId);
            setTextColor_LabelWidget(d->info, uiInputTextFocused_ColorId);
            addChild_Widget(w, iClob(page));
            addChild_Widget(w, iClob(makePadding_Widget(gap_UI)));
        }
        /* Tabs for input data. */
        iWidget *tabs = makeTabs_Widget(w);
        /* Make the tabs support vertical expansion based on content. */ {
            setFlags_Widget(tabs, resizeHeightOfChildren_WidgetFlag, iFalse);
            setFlags_Widget(tabs, arrangeHeight_WidgetFlag, iTrue);
            iWidget *tabPages = findChild_Widget(tabs, "tabs.pages");
            setFlags_Widget(tabPages, resizeHeightOfChildren_WidgetFlag, iFalse);
            setFlags_Widget(tabPages, arrangeHeight_WidgetFlag, iTrue);
        }
        setBackgroundColor_Widget(findChild_Widget(tabs, "tabs.buttons"), uiBackgroundSidebar_ColorId);
        setId_Widget(tabs, "upload.tabs");
        /* Text input. */ {
            iWidget *page = new_Widget();
            setFlags_Widget(page, arrangeSize_WidgetFlag, iTrue);
            d->input = new_InputWidget(0);
            setId_Widget(as_Widget(d->input), "upload.text");
            setFixedSize_Widget(as_Widget(d->input), init_I2(120 * gap_UI, -1));
            addChild_Widget(page, iClob(d->input));
            appendFramelessTabPage_Widget(tabs, iClob(page), "${heading.upload.text}", '1', 0);
        }
        /* File content. */ {
            iWidget *page = appendTwoColumnTabPage_Widget(tabs, "${heading.upload.file}", '2', &headings, &values);
            setBackgroundColor_Widget(page, uiBackgroundSidebar_ColorId);
            addChildFlags_Widget(headings, iClob(new_LabelWidget("${upload.file.name}", NULL)), frameless_WidgetFlag);
            d->filePathLabel = addChildFlags_Widget(values, iClob(new_LabelWidget(uiTextAction_ColorEscape "${upload.file.drophere}", NULL)), frameless_WidgetFlag);
            addChildFlags_Widget(headings, iClob(new_LabelWidget("${upload.file.size}", NULL)), frameless_WidgetFlag);
            d->fileSizeLabel = addChildFlags_Widget(values, iClob(new_LabelWidget("\u2014", NULL)), frameless_WidgetFlag);
            d->mime = new_InputWidget(0);
            setFixedSize_Widget(as_Widget(d->mime), init_I2(70 * gap_UI, -1));
            addTwoColumnDialogInputField_Widget(headings, values, "${upload.mime}", "upload.mime", iClob(d->mime));
        }
        /* Identity and Token. */ {
            addChild_Widget(w, iClob(makePadding_Widget(gap_UI)));
            iWidget *page = makeTwoColumns_Widget(&headings, &values);
            /* Identity. */
            const iArray *   identItems = makeIdentityItems_UploadWidget_(d);
            const iMenuItem *items      = constData_Array(identItems);
            const size_t     numItems   = size_Array(identItems);
            d->ident                    = makeMenuButton_LabelWidget("${upload.id}", items, numItems);
            setTextCStr_LabelWidget(d->ident, items[findWidestLabel_MenuItem(items, numItems)].label);
            //setFixedSize_Widget(as_Widget(d->ident), init_I2(50 * gap_UI, ));
            addChild_Widget(headings, iClob(makeHeading_Widget("${upload.id}")));
            setId_Widget(addChildFlags_Widget(values, iClob(d->ident), alignLeft_WidgetFlag), "upload.id");
            /* Token. */
            d->token = addTwoColumnDialogInputField_Widget(
                headings, values, "${upload.token}", "upload.token", iClob(new_InputWidget(0)));
            setHint_InputWidget(d->token, "${hint.upload.token}");
            setFixedSize_Widget(as_Widget(d->token), init_I2(50 * gap_UI, -1));            
            addChild_Widget(w, iClob(page));
        }
        /* Buttons. */ {
            addChild_Widget(w, iClob(makePadding_Widget(gap_UI)));
            iWidget *buttons = makeDialogButtons_Widget(actions, iElemCount(actions));
            setId_Widget(insertChildAfterFlags_Widget(buttons,
                                                      iClob(d->counter = new_LabelWidget("", NULL)),
                                                      0,
                                                      frameless_WidgetFlag),
                         "upload.counter");
            addChild_Widget(w, iClob(buttons));
        }
        resizeToLargestPage_Widget(tabs);
        arrange_Widget(w);
        setFixedSize_Widget(as_Widget(d->path),  init_I2(width_Widget(tabs) - width_Widget(d->info), -1));
        setFixedSize_Widget(as_Widget(d->mime),  init_I2(width_Widget(tabs) - 3 * gap_UI -
                                                            left_Rect(parent_Widget(d->mime)->rect), -1));
        setFixedSize_Widget(as_Widget(d->token), init_I2(width_Widget(tabs) - left_Rect(parent_Widget(d->token)->rect), -1));
        setFixedSize_Widget(as_Widget(d->ident), init_I2(width_Widget(d->token),
                                                         lineHeight_Text(uiLabel_FontId) + 2 * gap_UI));
        setFlags_Widget(as_Widget(d->token), expand_WidgetFlag, iTrue);
        setFocus_Widget(as_Widget(d->input));
    }
    setFont_InputWidget(d->input, FONT_ID(monospace_FontId, regular_FontStyle, uiSmall_FontSize));
    setUseReturnKeyBehavior_InputWidget(d->input, iFalse); /* traditional text editor */
    setLineLimits_InputWidget(d->input, 7, 20);
    setHint_InputWidget(d->input, "${hint.upload.text}");
    setBackupFileName_InputWidget(d->input, "uploadbackup");
    setBackupFileName_InputWidget(d->token, "uploadtoken"); /* TODO: site-specific config? */
    updateInputMaxHeight_UploadWidget_(d);
}

void deinit_UploadWidget(iUploadWidget *d) {
    releaseFile_UploadWidget_(d);
    deinit_Block(&d->idFingerprint);
    deinit_String(&d->filePath);
    deinit_String(&d->url);
    deinit_String(&d->originalUrl);
    iRelease(d->request);
}

static void remakeIdentityItems_UploadWidget_(iUploadWidget *d) {
    iWidget *dropMenu = findChild_Widget(findChild_Widget(as_Widget(d), "upload.id"), "menu");
    const iArray *items = makeIdentityItems_UploadWidget_(d);
    /* TODO: Make the following a utility method. */
    if (flags_Widget(dropMenu) & nativeMenu_WidgetFlag) {
        setNativeMenuItems_Widget(dropMenu, constData_Array(items), size_Array(items));
    }
    else {
        releaseChildren_Widget(dropMenu);
        makeMenuItems_Widget(dropMenu, constData_Array(items), size_Array(items));
    }
}

static void updateIdentityDropdown_UploadWidget_(iUploadWidget *d) {
    updateDropdownSelection_LabelWidget(
        findChild_Widget(as_Widget(d), "upload.id"),
        d->idMode == none_UploadIdentity ? " arg:0"
        : d->idMode == defaultForSite_UploadIdentity
            ? " arg:1"
            : format_CStr(" fp:%s", cstrCollect_String(hexEncode_Block(&d->idFingerprint))));
}

static uint16_t titanPortForUrl_(const iString *url) {
    uint16_t port = 0;
    const iString *root = collectNewRange_String(urlRoot_String(url));
    iUrl parts;
    init_Url(&parts, url);
    /* If the port is not specified, use the site-specific configuration. */
    if (isEmpty_Range(&parts.port) || equalCase_Rangecc(parts.scheme, "gemini")) {
        port = value_SiteSpec(root, titanPort_SiteSpecKey);
    }
    else {
        port = atoi(cstr_Rangecc(parts.port));
    }
    return port ? port : GEMINI_DEFAULT_PORT;
}

static void setUrlPort_UploadWidget_(iUploadWidget *d, const iString *url, uint16_t overridePort) {
    set_String(&d->originalUrl, url);
    iUrl parts;
    init_Url(&parts, url);
    setCStr_String(&d->url, "titan");
    appendRange_String(&d->url, (iRangecc){ parts.scheme.end, parts.host.end });
    appendFormat_String(&d->url, ":%u", overridePort ? overridePort : titanPortForUrl_(url));
    appendRange_String(&d->url, (iRangecc){ parts.path.start, constEnd_String(url) });
    const iRangecc siteRoot = urlRoot_String(&d->url);
    setTextCStr_LabelWidget(d->info, cstr_Rangecc((iRangecc){ urlHost_String(&d->url).start,
                                                              siteRoot.end }));
    /* From root onwards, the URL is editable. */
    setTextCStr_InputWidget(d->path,
                            cstr_Rangecc((iRangecc){ siteRoot.end, constEnd_String(&d->url) }));
    if (!cmp_String(text_InputWidget(d->path), "/")) {
        setTextCStr_InputWidget(d->path, ""); /* might as well show the hint */
    }
    if (isUsingPanelLayout_Mobile()) {
        arrange_Widget(as_Widget(d)); /* a wrapped label */
    }
    else {
        setFixedSize_Widget(as_Widget(d->path),
                            init_I2(width_Widget(findChild_Widget(as_Widget(d), "upload.tabs")) -
                                        width_Widget(d->info),
                                    -1));
    }
}

void setUrl_UploadWidget(iUploadWidget *d, const iString *url) {
    setUrlPort_UploadWidget_(d, url, 0);
    remakeIdentityItems_UploadWidget_(d);
    updateIdentityDropdown_UploadWidget_(d);
}

void setResponseViewer_UploadWidget(iUploadWidget *d, iDocumentWidget *doc) {
    d->viewer = doc;
}

void setText_UploadWidget(iUploadWidget *d, const iString *text) {
    setText_InputWidget(findChild_Widget(as_Widget(d), "upload.text"), text);    
}

static iWidget *acceptButton_UploadWidget_(iUploadWidget *d) {
    return lastChild_Widget(findChild_Widget(as_Widget(d), "dialogbuttons"));
}

#if 0
static void requestUpdated_UploadWidget_(iUploadWidget *d, iGmRequest *req) {
    if (!exchange_Atomic(&d->isRequestUpdated, iTrue)) {
        postCommand_Widget(d, "upload.request.updated reqid:%u", id_GmRequest(req));
    }
}
#endif

static void requestFinished_UploadWidget_(iUploadWidget *d, iGmRequest *req) {
    postCommand_Widget(d, "upload.request.finished reqid:%u", id_GmRequest(req));
}

static void updateFileInfo_UploadWidget_(iUploadWidget *d) {
    iFileInfo *info = iClob(new_FileInfo(&d->filePath));
    if (isDirectory_FileInfo(info)) {
        makeMessage_Widget("${heading.upload.error.file}",
                           "${upload.error.directory}",
                           (iMenuItem[]){ "${dlg.message.ok}", 0, 0, "message.ok" }, 1);
        clear_String(&d->filePath);
        d->fileSize = 0;
        return;
    }
    d->fileSize = size_FileInfo(info);
#if defined (iPlatformMobile)
    setTextCStr_LabelWidget(d->filePathLabel, cstr_Rangecc(baseName_Path(&d->filePath)));
#else
    setText_LabelWidget(d->filePathLabel, &d->filePath);
#endif
    setTextCStr_LabelWidget(d->fileSizeLabel, formatCStrs_Lang("num.bytes.n", d->fileSize));
    setTextCStr_InputWidget(d->mime, mediaType_Path(&d->filePath));
}

static void showOrHideUploadButton_UploadWidget_(iUploadWidget *d) {
    if (isUsingPanelLayout_Mobile()) {
        enableUploadButton_UploadWidget_(
            d, currentPanelIndex_Mobile(as_Widget(d)) != iInvalidPos || !isPortraitPhone_App());
    }
}

static const iString *requestUrl_UploadWidget_(const iUploadWidget *d) {
    const iRangecc siteRoot = urlRoot_String(&d->url);
    iString *reqUrl = collectNew_String();
    setRange_String(reqUrl, (iRangecc){ constBegin_String(&d->url), siteRoot.end });
    const iString *path = text_InputWidget(d->path);
    if (!startsWith_String(path, "/")) {
        appendCStr_String(reqUrl, "/");
    }
    append_String(reqUrl, path);
    return reqUrl;
}

static iBool processEvent_UploadWidget_(iUploadWidget *d, const SDL_Event *ev) {
    iWidget *w = as_Widget(d);
    const char *cmd = command_UserEvent(ev);
    if (isResize_UserEvent(ev) || equal_Command(cmd, "keyboard.changed")) {
        updateInputMaxHeight_UploadWidget_(d);
        showOrHideUploadButton_UploadWidget_(d);
    }
    else if (equal_Command(cmd, "panel.changed")) {
        showOrHideUploadButton_UploadWidget_(d);
        if (currentPanelIndex_Mobile(w) == 0) {
            setFocus_Widget(as_Widget(d->input));
        }
        else {
            setFocus_Widget(NULL);
        }
        refresh_Widget(d->input);
        return iFalse;
    }
#if defined (iPlatformAppleMobile)
    else if (deviceType_App() != desktop_AppDeviceType && equal_Command(cmd, "menu.opened")) {
        setFocus_Widget(NULL); /* overlaid text fields! */
        refresh_Widget(d->input);
        return iFalse;
    }
#endif
    else if (equal_Command(cmd, "upload.cancel")) {
        setupSheetTransition_Mobile(w, iFalse);
        destroy_Widget(w);
        return iTrue;
    }
    else if (isCommand_Widget(w, ev, "upload.setport")) {
        if (hasLabel_Command(cmd, "value")) {
            setValue_SiteSpec(collectNewRange_String(urlRoot_String(&d->originalUrl)),
                              titanPort_SiteSpecKey, arg_Command(cmd));
            setUrlPort_UploadWidget_(d, &d->originalUrl, arg_Command(cmd));
        }
        else {
            makeValueInput_Widget(root_Widget(w),
                                  collectNewFormat_String("%u", titanPortForUrl_(&d->originalUrl)),
                                  uiHeading_ColorEscape "${heading.uploadport}",
                                  "${dlg.uploadport.msg}",
                                  "${dlg.uploadport.set}",
                                  format_CStr("upload.setport ptr:%p", d));
        }
        return iTrue;
    }
    if (isCommand_Widget(w, ev, "upload.setid")) {
        if (hasLabel_Command(cmd, "fp")) {
            set_Block(&d->idFingerprint, collect_Block(hexDecode_Rangecc(range_Command(cmd, "fp"))));
            d->idMode = dropdown_UploadIdentity;
        }
        else if (arg_Command(cmd)) {
            clear_Block(&d->idFingerprint);
            d->idMode = defaultForSite_UploadIdentity;
        }
        else {
            clear_Block(&d->idFingerprint);
            d->idMode = none_UploadIdentity;
        }
        updateIdentityDropdown_UploadWidget_(d);
        return iTrue;
    }
    if (isCommand_Widget(w, ev, "upload.editmenu.open")) {
        setFocus_Widget(NULL);
        refresh_Widget(as_Widget(d->input));
        iWidget *editMenu = makeMenu_Widget(root_Widget(w), (iMenuItem[]){
            { select_Icon " ${menu.selectall}", 0, 0, "upload.text.selectall" },
            { export_Icon " ${menu.upload.export}", 0, 0, "upload.text.export" },
            { "---" },
            { delete_Icon " " uiTextCaution_ColorEscape "${menu.upload.delete}", 0, 0, "upload.text.delete" }
        }, 4);
        openMenu_Widget(editMenu, topLeft_Rect(bounds_Widget(as_Widget(d->input))));
        return iTrue;
    }
    if (isCommand_UserEvent(ev, "upload.text.export")) {
#if defined (iPlatformAppleMobile)
        openTextActivityView_iOS(text_InputWidget(d->input));
#endif
        return iTrue;
    }
    if (isCommand_UserEvent(ev, "upload.text.delete")) {
        if (argLabel_Command(command_UserEvent(ev), "confirmed")) {
            setTextCStr_InputWidget(d->input, "");
            setFocus_Widget(as_Widget(d->input));
        }
        else {
            openMenu_Widget(makeMenu_Widget(root_Widget(w), (iMenuItem[]){
                { delete_Icon " " uiTextCaution_ColorEscape "${menu.upload.delete.confirm}", 0, 0,
                    "upload.text.delete confirmed:1" }
            }, 1), zero_I2());
        }
        return iTrue;
    }
    if (isCommand_UserEvent(ev, "upload.text.selectall")) {
        setFocus_Widget(as_Widget(d->input));
        refresh_Widget(as_Widget(d->input));
        postCommand_Widget(d->input, "input.selectall");
        return iTrue;
    }
    if (isCommand_Widget(w, ev, "upload.accept")) {
        iBool isText;
        iWidget *tabs = findChild_Widget(w, "upload.tabs");
        if (tabs) {
            const size_t tabIndex = tabPageIndex_Widget(tabs, currentTabPage_Widget(tabs));
            isText = (tabIndex == 0);
        }
        else {
            const size_t panelIndex = currentPanelIndex_Mobile(w);
            if (panelIndex == iInvalidPos) {
                return iTrue;
            }
            isText = (currentPanelIndex_Mobile(w) == 0);
        }
        /* Make a GmRequest and send the data. */
        iAssert(d->request == NULL);
        iAssert(!isEmpty_String(&d->url));
        d->request = new_GmRequest(certs_App());
        setSendProgressFunc_GmRequest(d->request, updateProgress_UploadWidget_);
        setUserData_Object(d->request, d);
        setUrl_GmRequest(d->request, requestUrl_UploadWidget_(d));
        const iString     *site    = collectNewRange_String(urlRoot_String(&d->url));
        switch (d->idMode) {
            case none_UploadIdentity:
                /* Ensure no identity will be used for this specific URL. */
                signOut_GmCerts(certs_App(), url_GmRequest(d->request));
                setValueString_SiteSpec(site, titanIdentity_SiteSpecKey, collectNew_String());
                break;
            case dropdown_UploadIdentity: {
                iGmIdentity *ident = findIdentity_GmCerts(certs_App(), &d->idFingerprint);
                if (ident) {
                    setValueString_SiteSpec(site,
                                            titanIdentity_SiteSpecKey,
                                            collect_String(hexEncode_Block(&ident->fingerprint)));
                }
                break;
            }
            default:
                break;
        }
        if (d->idMode != none_UploadIdentity) {
            setIdentity_GmRequest(d->request, titanIdentityForUrl_(&d->url));
        }
        if (isText) {
            /* Uploading text. */
            setTitanData_GmRequest(d->request,
                                   collectNewCStr_String("text/plain"),
                                   utf8_String(text_InputWidget(d->input)),
                                   text_InputWidget(d->token));
        }
        else {
            /* Uploading a file. */
            iFile *f = iClob(new_File(&d->filePath));
            if (!open_File(f, readOnly_FileMode)) {
                makeMessage_Widget("${heading.upload.error.file}",
                                   "${upload.error.msg}",
                                   (iMenuItem[]){ "${dlg.message.ok}", 0, 0, "message.ok" }, 1);
                iReleasePtr(&d->request);
                return iTrue;
            }
            setTitanData_GmRequest(d->request,
                                   text_InputWidget(d->mime),
                                   collect_Block(readAll_File(f)),
                                   text_InputWidget(d->token));
            close_File(f);
        }
//        iConnect(GmRequest, d->request, updated,  d, requestUpdated_UploadWidget_);
        iConnect(GmRequest, d->request, finished, d, requestFinished_UploadWidget_);
        submit_GmRequest(d->request);
        /* The dialog will remain open until the request finishes, showing upload progress. */
        setFocus_Widget(NULL);
        setFlags_Widget(tabs, disabled_WidgetFlag, iTrue);
        setFlags_Widget(as_Widget(d->token), disabled_WidgetFlag, iTrue);
        setFlags_Widget(acceptButton_UploadWidget_(d), disabled_WidgetFlag, iTrue);
        return iTrue;
    }
    else if (isCommand_Widget(w, ev, "upload.request.updated") &&
             id_GmRequest(d->request) == argU32Label_Command(cmd, "reqid")) {
        setText_LabelWidget(d->counter,
                            collectNewFormat_String("%u", argU32Label_Command(cmd, "arg")));
    }
    else if (isCommand_Widget(w, ev, "upload.request.finished") &&
             id_GmRequest(d->request) == argU32Label_Command(cmd, "reqid")) {
        if (isSuccess_GmStatusCode(status_GmRequest(d->request))) {
            setBackupFileName_InputWidget(d->input, NULL); /* erased */
        }
        if (d->viewer) {
            takeRequest_DocumentWidget(d->viewer, d->request);
            d->request = NULL; /* DocumentWidget has it now. */
        }
        setupSheetTransition_Mobile(w, iFalse);
        releaseFile_UploadWidget_(d);
        destroy_Widget(w);
        return iTrue;        
    }
    else if (isCommand_Widget(w, ev, "input.resized")) {
        if (!isUsingPanelLayout_Mobile()) {
            resizeToLargestPage_Widget(findChild_Widget(w, "upload.tabs"));
            arrange_Widget(w);
            refresh_Widget(w);
            return iTrue;
        }
        else {
            refresh_Widget(as_Widget(d->input));
        }
    }
    else if (isCommand_Widget(w, ev, "upload.pickfile")) {
#if defined (iPlatformAppleMobile)
        if (hasLabel_Command(cmd, "path")) {
            releaseFile_UploadWidget_(d);
            set_String(&d->filePath, collect_String(suffix_Command(cmd, "path")));
            updateFileInfo_UploadWidget_(d);
        }
        else {
            pickFile_iOS(format_CStr("upload.pickfile ptr:%p", d));
        }
#endif
        return iTrue;
    }
    if (ev->type == SDL_DROPFILE) {
        /* Switch to File tab. */
        iWidget *tabs = findChild_Widget(w, "upload.tabs");
        showTabPage_Widget(tabs, tabPage_Widget(tabs, 1));
        releaseFile_UploadWidget_(d);
        setCStr_String(&d->filePath, ev->drop.file);
        updateFileInfo_UploadWidget_(d);
        return iTrue;
    }
    return processEvent_Widget(w, ev);
}

//static void draw_UploadWidget_(const iUploadWidget *d) {
//    draw_Widget(constAs_Widget(d));
//}

iBeginDefineSubclass(UploadWidget, Widget)
    .processEvent = (iAny *) processEvent_UploadWidget_,
    .draw         = draw_Widget,
iEndDefineSubclass(UploadWidget)
