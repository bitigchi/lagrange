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

#include "mobile.h"

#include "app.h"
#include "certlistwidget.h"
#include "command.h"
#include "defs.h"
#include "inputwidget.h"
#include "labelwidget.h"
#include "root.h"
#include "text.h"
#include "widget.h"
#include "window.h"

#if defined (iPlatformAppleMobile)
#   include "ios.h"
#endif

const iToolbarActionSpec toolbarActions_Mobile[max_ToolbarAction] = {
    { backArrow_Icon, "${menu.back}", "navigate.back" },
    { forwardArrow_Icon, "${menu.forward}", "navigate.forward" },
    { home_Icon, "${menu.home}", "navigate.home" },
    { upArrow_Icon, "${menu.parent}", "navigate.parent" },
    { reload_Icon, "${menu.reload}", "navigate.reload" },
    { add_Icon, "${menu.newtab}", "tabs.new" },
    { close_Icon, "${menu.closetab}", "tabs.close" },
    { bookmark_Icon, "${menu.page.bookmark}", "bookmark.add" },
    { globe_Icon, "${menu.page.translate}", "document.translate" },
    { upload_Icon, "${menu.page.upload}", "document.upload" },
    { edit_Icon, "${menu.page.upload.edit}", "document.upload copy:1" },
    { magnifyingGlass_Icon, "${menu.find}", "focus.set id:find.input" },
    { gear_Icon, "${menu.settings}", "preferences" },
    { leftHalf_Icon, "${menu.sidebar.left}", "sidebar.toggle" },        
};

iBool isUsingPanelLayout_Mobile(void) {
    return deviceType_App() != desktop_AppDeviceType;
}

#define topPanelMinWidth_Mobile     (80 * gap_UI)

static iBool isSideBySideLayout_(void) {
    /* Minimum is an even split. */
    const int safeWidth = safeRect_Root(get_Root()).size.x;
    if (safeWidth / 2 < topPanelMinWidth_Mobile) {
        return iFalse;
    }
    if (deviceType_App() == phone_AppDeviceType) {
        return isLandscape_App();
    }
    /* Tablet may still be too narrow. */
    return numRoots_Window(get_Window()) == 1;
}

static enum iFontId labelFont_(void) {
    return deviceType_App() == phone_AppDeviceType ? uiLabelBig_FontId : uiLabelMedium_FontId;
}

static enum iFontId labelBoldFont_(void) {
    return deviceType_App() == phone_AppDeviceType ? uiLabelBigBold_FontId : uiLabelMediumBold_FontId;
}

static void updatePanelSheetMetrics_(iWidget *sheet) {
    iWidget *navi       = findChild_Widget(sheet, "panel.navi");
    int      naviHeight = lineHeight_Text(labelFont_()) + 4 * gap_UI;
#if defined (iPlatformMobile)
    float left = 0.0f, right = 0.0f, top = 0.0f, bottom = 0.0f;
#if defined (iPlatformAppleMobile)
    safeAreaInsets_iOS(&left, &top, &right, &bottom);
#endif
    setPadding_Widget(sheet, left, 0, right, 0);
    navi->rect.pos = init_I2(left, top);
    iConstForEach(PtrArray, i, findChildren_Widget(sheet, "panel.toppad")) {
        iWidget *pad = *i.value;
        setFixedSize_Widget(pad, init1_I2(naviHeight));
    }
#endif
    setFixedSize_Widget(navi, init_I2(-1, naviHeight));
}

static iWidget *findDetailStack_(iWidget *topPanel) {
    return findChild_Widget(parent_Widget(topPanel), "detailstack");
}

static void unselectAllPanelButtons_(iWidget *topPanel) {
    iForEach(ObjectList, i, children_Widget(topPanel)) {
        if (isInstance_Object(i.object, &Class_LabelWidget)) {
            iLabelWidget *label = i.object;
            if (!cmp_String(command_LabelWidget(label), "panel.open")) {
                setFlags_Widget(i.object, selected_WidgetFlag, iFalse);
            }
        }
    }
}

static iWidget *findTitleLabel_(iWidget *panel) {
    iForEach(ObjectList, i, children_Widget(panel)) {
        iWidget *child = i.object;
        if (flags_Widget(child) & collapse_WidgetFlag &&
            isInstance_Object(child, &Class_LabelWidget)) {
            return child;
        }
    }
    return NULL;
}

static void updateCertListHeight_(iWidget *detailStack) {
    iWidget *certList = findChild_Widget(detailStack, "certlist");
    if (certList) {
        setFixedSize_Widget(certList,
                            init_I2(-1,
                                    -1 * gap_UI + bottom_Rect(safeRect_Root(certList->root)) -
                                        top_Rect(boundsWithoutVisualOffset_Widget(certList))));
    }
}

static iBool mainDetailSplitHandler_(iWidget *mainDetailSplit, const char *cmd) {
    if (equal_Command(cmd, "window.resized")) {
        const iBool  isPortrait   = (deviceType_App() == phone_AppDeviceType && isPortrait_App());
        const iRect  safeRoot     = safeRect_Root(mainDetailSplit->root);
        iWidget *    sheet        = parent_Widget(mainDetailSplit);
        iWidget *    navi         = findChild_Widget(sheet, "panel.navi");
        iWidget *    detailStack  = findChild_Widget(mainDetailSplit, "detailstack");
        const size_t numPanels    = childCount_Widget(detailStack);
        const iBool  isSideBySide = isSideBySideLayout_() && numPanels > 0;
        setPos_Widget(mainDetailSplit, topLeft_Rect(safeRoot));
        setFixedSize_Widget(mainDetailSplit, safeRoot.size);
        setFlags_Widget(mainDetailSplit, arrangeHorizontal_WidgetFlag, isSideBySide);
        setFlags_Widget(detailStack, expand_WidgetFlag, isSideBySide);
        setFlags_Widget(detailStack, hidden_WidgetFlag, numPanels == 0);
        iWidget *topPanel = findChild_Widget(mainDetailSplit, "panel.top");
        const int pad = isPortrait ? 0 : 3 * gap_UI;
        if (isSideBySide) {
            iAssert(topPanel);
            topPanel->rect.size.x = iMax(topPanelMinWidth_Mobile,
                                         (deviceType_App() == phone_AppDeviceType ?
                                          safeRoot.size.x * 2 / 5 : safeRoot.size.x / 3));
        }
        if (deviceType_App() == tablet_AppDeviceType) {
            setPadding_Widget(topPanel, pad, 0, pad, pad);
#if 0
            if (numPanels == 0) {
                setFlags_Widget(sheet, centerHorizontal_WidgetFlag, iTrue);
                const int sheetWidth = iMin(safeRoot.size.x, safeRoot.size.y);
                mainDetailSplit->rect.size.x = sheetWidth;
                setFixedSize_Widget(sheet, init_I2(sheetWidth, -1));
                setFixedSize_Widget(navi, init_I2(sheetWidth, -1));
            }
#endif
        }
        iWidget *detailTitle = findChild_Widget(navi, "detailtitle"); {
            setPos_Widget(detailTitle, init_I2(width_Widget(topPanel), 0));
            setFixedSize_Widget(detailTitle,
                                init_I2(width_Widget(detailStack), height_Widget(navi)));
            setFlags_Widget(detailTitle, hidden_WidgetFlag, !isSideBySide);
        }
        iForEach(ObjectList, i, children_Widget(detailStack)) {
            iWidget *panel = i.object;
            setFlags_Widget(findTitleLabel_(panel), hidden_WidgetFlag, isSideBySide);
            setFlags_Widget(panel, leftEdgeDraggable_WidgetFlag, !isSideBySide);
            if (isSideBySide) {
                setVisualOffset_Widget(panel, 0, 0, 0);
            }
            setPadding_Widget(panel, pad, 0, pad, pad + bottomSafeInset_Mobile());
        }
        arrange_Widget(mainDetailSplit);
        updateCertListHeight_(detailStack);
    }
    else if (equal_Command(cmd, "mouse.clicked") && arg_Command(cmd)) {
        if (focus_Widget() && class_Widget(focus_Widget()) == &Class_InputWidget) {
            setFocus_Widget(NULL);
            return iTrue;
        }
    }
    return iFalse;
}

size_t currentPanelIndex_Mobile(const iWidget *panels) {
    size_t index = 0;
    iConstForEach(ObjectList, i, children_Widget(findChild_Widget(panels, "detailstack"))) {
        const iWidget *child = i.object;
        if (isVisible_Widget(child)) {
            return index;
        }
        index++;
    }
    return iInvalidPos;
}

iWidget *panel_Mobile(const iWidget *panels, size_t index) {
    return child_Widget(findChild_Widget(panels, "detailstack"), index);
}

static iBool topPanelHandler_(iWidget *topPanel, const char *cmd) {
    const iBool isPortrait = !isSideBySideLayout_();
    if (equal_Command(cmd, "panel.open")) {
        /* This command is sent by the button that opens the panel. */
        iWidget *button = pointer_Command(cmd);        
        iWidget *panel = userData_Object(button);
        unselectAllPanelButtons_(topPanel);
        int panelIndex = -1;
        size_t childIndex = 0;
        iForEach(ObjectList, i, children_Widget(findDetailStack_(topPanel))) {
            iWidget *child = i.object;
            setFlags_Widget(child, hidden_WidgetFlag | disabled_WidgetFlag, child != panel);
            /* Animate the current panel in. */
            if (child == panel && isPortrait) {
                setupSheetTransition_Mobile(panel, iTrue);
                panelIndex = (int) childIndex;
            }
            childIndex++;
        }
        iLabelWidget *detailTitle =
            findChild_Widget(parent_Widget(parent_Widget(topPanel)), "detailtitle");
//        setFlags_Widget(as_Widget(detailTitle), hidden_WidgetFlag, !isSideBySideLayout_());
        setFont_LabelWidget(detailTitle, uiLabelLargeBold_FontId);
        setTextColor_LabelWidget(detailTitle, uiHeading_ColorId);
        setText_LabelWidget(detailTitle, text_LabelWidget((iLabelWidget *) findTitleLabel_(panel)));
        setFlags_Widget(button, selected_WidgetFlag, iTrue);
        postCommand_Widget(topPanel, "panel.changed arg:%d", panelIndex);
        updateCertListHeight_(findDetailStack_(topPanel));
        return iTrue;
    }
    if (equal_Command(cmd, "swipe.back")) {
        postCommand_App("panel.close");
        return iTrue;
    }
    if (equal_Command(cmd, "panel.close")) {
        iBool wasClosed = iFalse;
        if (isPortrait) {
            iForEach(ObjectList, i, children_Widget(findDetailStack_(topPanel))) {
                iWidget *child = i.object;
                if (!cmp_String(id_Widget(child), "panel") && isVisible_Widget(child)) {
    //                closeMenu_Widget(child);
                    setupSheetTransition_Mobile(child, iFalse);
                    setFlags_Widget(child, hidden_WidgetFlag | disabled_WidgetFlag, iTrue);
                    setFocus_Widget(NULL);
                    updateTextCStr_LabelWidget(findWidget_App("panel.back"), "Back");
                    wasClosed = iTrue;
                    postCommand_Widget(topPanel, "panel.changed arg:-1");
                }
            }
        }
        unselectAllPanelButtons_(topPanel);
        if (!wasClosed) {
            /* TODO: Should come up with a more general-purpose approach here. */
            if (findWidget_App("prefs")) {
                postCommand_App("prefs.dismiss");
            }
            else if (findWidget_App("upload")) {
                postCommand_App("upload.cancel");
            }
            else if (findWidget_App("bmed.sidebar") || findWidget_App("bmed.create") ||
                     findWidget_App("bmed")) {
                postCommand_App("bmed.cancel");
            }
            else if (findWidget_App("ident")) {
                postCommand_Widget(topPanel, "ident.cancel");
            }
            else if (findWidget_App("xlt")) {
                postCommand_Widget(topPanel, "translation.cancel");
            }
            else {
                postCommand_Widget(topPanel, "cancel");
            }
        }
        return iTrue;
    }
    if (equal_Command(cmd, "document.changed")) {
        postCommand_App("prefs.dismiss");
        return iFalse;
    }
    if (equal_Command(cmd, "window.resized")) {
        // sheet > mdsplit > panel.top
        updatePanelSheetMetrics_(parent_Widget(parent_Widget(topPanel)));
    }
    return iFalse;
}

#if 0
static iBool isTwoColumnPage_(iWidget *d) {
    if (cmp_String(id_Widget(d), "dialogbuttons") == 0 ||
        cmp_String(id_Widget(d), "prefs.tabs") == 0) {
        return iFalse;
    }
    if (class_Widget(d) == &Class_Widget && childCount_Widget(d) == 2) {
        return class_Widget(child_Widget(d, 0)) == &Class_Widget &&
               class_Widget(child_Widget(d, 1)) == &Class_Widget;
    }
    return iFalse;
}

static iBool isOmittedPref_(const iString *id) {
    static const char *omittedPrefs[] = {
        "prefs.userfont",
        "prefs.animate",
        "prefs.smoothscroll",
        "prefs.imageloadscroll",
        "prefs.pinsplit",
        "prefs.retainwindow",
        "prefs.ca.file",
        "prefs.ca.path",
    };
    iForIndices(i, omittedPrefs) {
        if (cmp_String(id, omittedPrefs[i]) == 0) {
            return iTrue;
        }
    }
    return iFalse;
}

enum iPrefsElement {
    panelTitle_PrefsElement,
    heading_PrefsElement,
    toggle_PrefsElement,
    dropdown_PrefsElement,
    radioButton_PrefsElement,
    textInput_PrefsElement,
};

static iAnyObject *addPanelChild_(iWidget *panel, iAnyObject *child, int64_t flags,
                                  enum iPrefsElement elementType,
                                  enum iPrefsElement precedingElementType) {
    /* Erase redundant/unused headings. */
    if (precedingElementType == heading_PrefsElement &&
        (!child || (elementType == heading_PrefsElement || elementType == radioButton_PrefsElement))) {
        iRelease(removeChild_Widget(panel, lastChild_Widget(panel)));
        if (!cmp_String(id_Widget(constAs_Widget(lastChild_Widget(panel))), "padding")) {
            iRelease(removeChild_Widget(panel, lastChild_Widget(panel)));
        }
    }
    if (child) {
        /* Insert padding between different element types. */
        if (precedingElementType != panelTitle_PrefsElement) {
            if (elementType == heading_PrefsElement ||
                (elementType == toggle_PrefsElement &&
                 precedingElementType != toggle_PrefsElement &&
                 precedingElementType != heading_PrefsElement) ||
                (elementType == dropdown_PrefsElement &&
                 precedingElementType != dropdown_PrefsElement &&
                 precedingElementType != heading_PrefsElement) ||
                (elementType == textInput_PrefsElement &&
                 precedingElementType != textInput_PrefsElement &&
                 precedingElementType != heading_PrefsElement)) {
                addChild_Widget(panel, iClob(makePadding_Widget(lineHeight_Text(labelFont_()))));
            }
        }
        if ((elementType == toggle_PrefsElement && precedingElementType != toggle_PrefsElement) ||
            (elementType == textInput_PrefsElement && precedingElementType != textInput_PrefsElement) ||
            (elementType == dropdown_PrefsElement && precedingElementType != dropdown_PrefsElement) ||
            (elementType == radioButton_PrefsElement && precedingElementType == heading_PrefsElement)) {
            flags |= borderTop_WidgetFlag;
        }
        return addChildFlags_Widget(panel, child, flags);
    }
    return NULL;
}

static void stripTrailingColon_(iLabelWidget *label) {
    const iString *text = text_LabelWidget(label);
    if (endsWith_String(text, ":")) {
        iString *mod = copy_String(text);
        removeEnd_String(mod, 1);
        updateText_LabelWidget(label, mod);
        delete_String(mod);
    }
}
#endif

static iLabelWidget *makePanelButton_(const char *text, const char *command) {
    iLabelWidget *btn = new_LabelWidget(text, command);
    setFlags_Widget(as_Widget(btn),
                    borderTop_WidgetFlag | borderBottom_WidgetFlag | alignLeft_WidgetFlag |
                        frameless_WidgetFlag | extraPadding_WidgetFlag,
                    iTrue);
    checkIcon_LabelWidget(btn);
    setFont_LabelWidget(btn, labelFont_());
    setTextColor_LabelWidget(btn, uiTextStrong_ColorId);
    setBackgroundColor_Widget(as_Widget(btn), uiBackgroundSidebar_ColorId);
    return btn;
}

static iWidget *makeValuePadding_(iWidget *value) {
    iInputWidget *input = isInstance_Object(value, &Class_InputWidget) ? (iInputWidget *) value : NULL;
    if (input) {
        setFont_InputWidget(input, labelFont_());
        setContentPadding_InputWidget(input, 3 * gap_UI, 3 * gap_UI);
    }
    iWidget *pad = new_Widget();
    setBackgroundColor_Widget(pad, uiBackgroundSidebar_ColorId);
    setPadding_Widget(pad, 0, 1 * gap_UI, 0, 1 * gap_UI);
    addChild_Widget(pad, iClob(value));
    setFlags_Widget(pad,
                    borderTop_WidgetFlag | borderBottom_WidgetFlag | arrangeVertical_WidgetFlag |
                        resizeToParentWidth_WidgetFlag | resizeWidthOfChildren_WidgetFlag |
                        arrangeHeight_WidgetFlag,
                    iTrue);
    return pad;
}

static iWidget *makeValuePaddingWithHeading_(iLabelWidget *heading, iWidget *value) {
    const iBool isInput = isInstance_Object(value, &Class_InputWidget);
    iWidget *div = new_Widget();
    setFlags_Widget(div,
                    borderTop_WidgetFlag | borderBottom_WidgetFlag | arrangeHeight_WidgetFlag |
                    resizeWidthOfChildren_WidgetFlag |
                    arrangeHorizontal_WidgetFlag, iTrue);
    setBackgroundColor_Widget(div, uiBackgroundSidebar_ColorId);
    setPadding_Widget(div, gap_UI, gap_UI, 4 * gap_UI, gap_UI);
    addChildFlags_Widget(div, iClob(heading), 0);
    setPadding1_Widget(as_Widget(heading), 0);
    //setFixedSize_Widget(as_Widget(heading), init_I2(-1, height_Widget(value)));
    setFont_LabelWidget(heading, labelFont_());
    setTextColor_LabelWidget(heading, uiTextStrong_ColorId);
    if (isInput && ~value->flags & fixedWidth_WidgetFlag) {
        addChildFlags_Widget(div, iClob(value), expand_WidgetFlag);
    }
    else if (isInstance_Object(value, &Class_LabelWidget) &&
             cmp_String(command_LabelWidget((iLabelWidget *) value), "toggle")) {
        addChildFlags_Widget(div, iClob(value), expand_WidgetFlag);
        /* TODO: This doesn't work? */
//        setCommand_LabelWidget(heading,
//                               collectNewFormat_String("!%s ptr:%p",
//                                           cstr_String(command_LabelWidget((iLabelWidget *) value)),
//                                           value));
    }
    else {
        addChildFlags_Widget(div, iClob(new_Widget()), expand_WidgetFlag);
        addChild_Widget(div, iClob(value));
    }
//    printTree_Widget(div);
    return div;
}

static iWidget *addChildPanel_(iWidget *parent, iLabelWidget *panelButton,
                               const iString *titleText) {
    iWidget *panel = new_Widget();
    setId_Widget(panel, "panel");
    setUserData_Object(panelButton, panel);
    setBackgroundColor_Widget(panel, uiBackground_ColorId);
    setDrawBufferEnabled_Widget(panel, iTrue);
    setId_Widget(addChild_Widget(panel, iClob(makePadding_Widget(0))), "panel.toppad");
    if (titleText) {
        iLabelWidget *title =
            addChildFlags_Widget(panel,
                                 iClob(new_LabelWidget(cstr_String(titleText), NULL)),
                                 alignLeft_WidgetFlag | frameless_WidgetFlag);
        setFont_LabelWidget(title, uiLabelLargeBold_FontId);
        setTextColor_LabelWidget(title, uiHeading_ColorId);
    }
    addChildFlags_Widget(parent,
                         iClob(panel),
                         focusRoot_WidgetFlag | hidden_WidgetFlag | disabled_WidgetFlag |
                             arrangeVertical_WidgetFlag | resizeWidthOfChildren_WidgetFlag |
                             arrangeHeight_WidgetFlag | overflowScrollable_WidgetFlag |
                             drawBackgroundToBottom_WidgetFlag |
                             horizontalOffset_WidgetFlag | commandOnClick_WidgetFlag);
    return panel;
}

//void finalizeSheet_Mobile(iWidget *sheet) {
//    arrange_Widget(sheet);
//    postRefresh_App();
//}

static size_t countItems_(const iMenuItem *itemsNullTerminated) {
    size_t num = 0;
    for (; itemsNullTerminated->label; num++, itemsNullTerminated++) {}
    return num;
}

static iBool dropdownHeadingHandler_(iWidget *d, const char *cmd) {
    if (isVisible_Widget(d) &&
        equal_Command(cmd, "mouse.clicked") && contains_Widget(d, coord_Command(cmd)) &&
        arg_Command(cmd)) {
        postCommand_Widget(userData_Object(d),
                           cstr_String(command_LabelWidget(userData_Object(d))));
        return iTrue;
    }
    return iFalse;
}

static iBool inputHeadingHandler_(iWidget *d, const char *cmd) {
    if (isVisible_Widget(d) &&
        equal_Command(cmd, "mouse.clicked") && contains_Widget(d, coord_Command(cmd)) &&
        arg_Command(cmd)) {
        setFocus_Widget(userData_Object(d));
        return iTrue;
    }
    return iFalse;
}

void makePanelItem_Mobile(iWidget *panel, const iMenuItem *item) {
    iWidget *     widget  = NULL;
    iLabelWidget *heading = NULL;
    iWidget *     value   = NULL;
    const char *  spec    = item->label;
    const char *  id      = cstr_Command(spec, "id");
    const char *  label   = hasLabel_Command(spec, "text")
                                ? suffixPtr_Command(spec, "text")
                                : format_CStr("${%s}", id);
    if (hasLabel_Command(spec, "device") && deviceType_App() != argLabel_Command(spec, "device")) {
        return;
    }
    if (equal_Command(spec, "title")) {
        iLabelWidget *title = addChildFlags_Widget(panel,
                                                   iClob(new_LabelWidget(label, NULL)),
                                                   alignLeft_WidgetFlag | frameless_WidgetFlag |
                                                   collapse_WidgetFlag);
        setFont_LabelWidget(title, uiLabelLargeBold_FontId);
        setTextColor_LabelWidget(title, uiHeading_ColorId);
//        setAllCaps_LabelWidget(title, iTrue);
        setId_Widget(as_Widget(title), id);
    }
    else if (equal_Command(spec, "heading")) {
        addChild_Widget(panel, iClob(makePadding_Widget(lineHeight_Text(labelFont_()))));
        heading = makeHeading_Widget(label);
        setAllCaps_LabelWidget(heading, iTrue);
        setRemoveTrailingColon_LabelWidget(heading, iTrue);
        addChild_Widget(panel, iClob(heading));
        setId_Widget(as_Widget(heading), id);
    }    
    else if (equal_Command(spec, "toggle")) {
        iLabelWidget *toggle = (iLabelWidget *) makeToggle_Widget(id);
        setFont_LabelWidget(toggle, labelFont_());
        widget = makeValuePaddingWithHeading_(heading = makeHeading_Widget(label),
                                              as_Widget(toggle));
    }
    else if (equal_Command(spec, "dropdown")) {
        const iMenuItem *dropItems = item->data;
        iLabelWidget *drop = makeMenuButton_LabelWidget(dropItems[0].label,
                                                        dropItems, countItems_(dropItems));
        value = as_Widget(drop);
        setFont_LabelWidget(drop, labelFont_());
        setFlags_Widget(as_Widget(drop),
                        alignRight_WidgetFlag | noBackground_WidgetFlag |
                            frameless_WidgetFlag, iTrue);
        setId_Widget(as_Widget(drop), id);
        widget = makeValuePaddingWithHeading_(heading = makeHeading_Widget(label), as_Widget(drop));
        setCommandHandler_Widget(widget, dropdownHeadingHandler_);
        widget->padding[2] = gap_UI;
        setUserData_Object(widget, drop);
    }
    else if (equal_Command(spec, "radio") || equal_Command(spec, "buttons")) {
        const iBool isRadio = equal_Command(spec, "radio");
        const iBool isHorizontal = argLabel_Command(spec, "horizontal");
        addChild_Widget(panel, iClob(makePadding_Widget(lineHeight_Text(labelFont_()))));
        iLabelWidget *head = makeHeading_Widget(label);
        setAllCaps_LabelWidget(head, iTrue);
        setRemoveTrailingColon_LabelWidget(head, iTrue);
        addChild_Widget(panel, iClob(head));
        widget = new_Widget();
        setBackgroundColor_Widget(widget, uiBackgroundSidebar_ColorId);
        const int hPad = (isHorizontal ? 0 : 1);
        setPadding_Widget(widget, hPad * gap_UI, 2 * gap_UI, hPad * gap_UI, 2 * gap_UI);
        setFlags_Widget(widget,
                        borderTop_WidgetFlag |
                            borderBottom_WidgetFlag |
                            (isHorizontal ? arrangeHorizontal_WidgetFlag : arrangeVertical_WidgetFlag) |
                            arrangeHeight_WidgetFlag |
                            resizeToParentWidth_WidgetFlag |
                            resizeWidthOfChildren_WidgetFlag,
                        iTrue);
        setId_Widget(widget, id);
        iBool isFirst = iTrue;
        for (const iMenuItem *radioItem = item->data; radioItem->label; radioItem++) {
            if (!isHorizontal && !isFirst) {
                /* The separator is padded from the left so we need two. */
                iWidget *sep = new_Widget();
                iWidget *sep2 = new_Widget();
                addChildFlags_Widget(sep, iClob(sep2), 0);
                setFlags_Widget(sep, arrangeHeight_WidgetFlag | resizeWidthOfChildren_WidgetFlag, iTrue);
                setBackgroundColor_Widget(sep2, uiSeparator_ColorId);
                setFixedSize_Widget(sep2, init_I2(-1, gap_UI / 4));
                setPadding_Widget(sep, 5 * gap_UI, 0, 0, 0);
                addChildFlags_Widget(widget, iClob(sep), 0);
            }
            isFirst = iFalse;
            const char *  radId = cstr_Command(radioItem->label, "id");
            int64_t       flags = noBackground_WidgetFlag | frameless_WidgetFlag;
            if (!isHorizontal) {
                flags |= alignLeft_WidgetFlag;
            }
            iLabelWidget *button;
            if (isRadio) {
                const char *radLabel =
                    hasLabel_Command(radioItem->label, "label")
                        ? format_CStr("${%s}",
                                      cstr_Command(radioItem->label, "label"))
                        : suffixPtr_Command(radioItem->label, "text");
                button = new_LabelWidget(radLabel, radioItem->command);
                flags |= radio_WidgetFlag;
            }
            else {
                button = (iLabelWidget *) makeToggle_Widget(radId);
                setTextCStr_LabelWidget(button, format_CStr("${%s}", radId));
                setFlags_Widget(as_Widget(button), fixedWidth_WidgetFlag, iFalse);
            }
            setId_Widget(as_Widget(button), radId);
            setFont_LabelWidget(button, deviceType_App() == phone_AppDeviceType ?
                                (isHorizontal ? uiLabelMedium_FontId : uiLabelBig_FontId) : labelFont_());
            setCheckMark_LabelWidget(button, !isHorizontal);
            setPadding_Widget(as_Widget(button), gap_UI, 1 * gap_UI, 0, 1 * gap_UI);
            updateSize_LabelWidget(button);
            setPadding_Widget(widget, 0, 0, 0, 0);
            addChildFlags_Widget(widget, iClob(button), flags);
        }
    }
    else if (equal_Command(spec, "input")) {
        iInputWidget *input = new_InputWidget(argU32Label_Command(spec, "maxlen"));
        if (hasLabel_Command(spec, "hint")) {
            setHint_InputWidget(input, cstr_Lang(cstr_Command(spec, "hint")));
        }
        setId_Widget(as_Widget(input), id);
        setUrlContent_InputWidget(input, argLabel_Command(spec, "url"));
        setSelectAllOnFocus_InputWidget(input, argLabel_Command(spec, "selectall"));        
        setFont_InputWidget(input, labelFont_());
        if (argLabel_Command(spec, "noheading")) {
            widget = makeValuePadding_(as_Widget(input));
            setFlags_Widget(widget, expand_WidgetFlag, iTrue);
        }
        else {
            setFlags_Widget(as_Widget(input), alignRight_WidgetFlag, iTrue);
            setContentPadding_InputWidget(input, 3 * gap_UI, 0);
            if (hasLabel_Command(spec, "unit")) {
                iWidget *unit = addChildFlags_Widget(
                    as_Widget(input),
                    iClob(new_LabelWidget(
                        format_CStr("${%s}", cstr_Command(spec, "unit")), NULL)),
                    frameless_WidgetFlag | moveToParentRightEdge_WidgetFlag |
                        resizeToParentHeight_WidgetFlag);
                setContentPadding_InputWidget(input, -1, width_Widget(unit) - 4 * gap_UI);
            }
            widget = makeValuePaddingWithHeading_(heading = makeHeading_Widget(label),
                                                  as_Widget(input));
            setCommandHandler_Widget(widget, inputHeadingHandler_);
            setUserData_Object(widget, input);
        }
    }
    else if (equal_Command(spec, "certlist")) {
        iCertListWidget *certList = new_CertListWidget();
        iListWidget *list = (iListWidget *) certList;
        setBackgroundColor_Widget(as_Widget(list), uiBackgroundSidebar_ColorId);
        widget = as_Widget(certList);
        updateItems_CertListWidget(certList);
        invalidate_ListWidget(list);
    }
    else if (equal_Command(spec, "button")) {
        widget = as_Widget(heading = makePanelButton_(label, item->command));
        setFlags_Widget(widget, selected_WidgetFlag, argLabel_Command(spec, "selected") != 0);
    }
    else if (equal_Command(spec, "label")) {
        iLabelWidget *lab = new_LabelWidget(label, NULL);
        widget = as_Widget(lab);
        setId_Widget(widget, id);
        setWrap_LabelWidget(lab, !argLabel_Command(spec, "nowrap"));
        setFlags_Widget(widget,
                        fixedHeight_WidgetFlag |
                            (!argLabel_Command(spec, "frame") ? frameless_WidgetFlag : 0),
                        iTrue);
        if (argLabel_Command(spec, "font")) {
            setFont_LabelWidget(lab, argLabel_Command(spec, "font"));
        }
    }
    else if (equal_Command(spec, "padding")) {
        float height = 1.5f;
        if (hasLabel_Command(spec, "arg")) {
            height *= argfLabel_Command(spec, "arg");
        }
        widget = makePadding_Widget(lineHeight_Text(labelFont_()) * height);
    }
    /* Apply common styling to the heading. */
    if (heading) {
        setRemoveTrailingColon_LabelWidget(heading, iTrue);
        const iChar icon = toInt_String(string_Command(item->label, "icon"));
        if (icon) {
            setIcon_LabelWidget(heading, icon);
        }
        if (value && as_Widget(heading) != value) {
            as_Widget(heading)->sizeRef = value; /* heading height matches value widget */
        }
    }
    if (widget) {
        setFlags_Widget(widget,
                        collapse_WidgetFlag | hidden_WidgetFlag,
                        argLabel_Command(spec, "collapse") != 0);
        addChild_Widget(panel, iClob(widget));
    }
}

void makePanelItems_Mobile(iWidget *panel, const iMenuItem *itemsNullTerminated) {
    for (const iMenuItem *item = itemsNullTerminated; item->label; item++) {
        makePanelItem_Mobile(panel, item);
    }
}

static const iMenuItem *findDialogCancelAction_(const iMenuItem *items, size_t n) {
    if (n <= 1) {
        return NULL;
    }
    for (size_t i = 0; i < n; i++) {
        if (!iCmpStr(items[i].label, "${cancel}") || !iCmpStr(items[i].label, "${close}")) {
            return &items[i];
        }
    }
    return NULL;
}

iWidget *makePanels_Mobile(const char *id,
                           const iMenuItem *itemsNullTerminated,
                           const iMenuItem *actions, size_t numActions) {
    return makePanelsParent_Mobile(get_Root()->widget, id, itemsNullTerminated, actions, numActions);
}

iWidget *makePanelsParent_Mobile(iWidget *parentWidget,
                                 const char *id,
                                 const iMenuItem *itemsNullTerminated,
                                 const iMenuItem *actions, size_t numActions) {
    iWidget *panels = new_Widget();
    setId_Widget(panels, id);
    initPanels_Mobile(panels, parentWidget, itemsNullTerminated, actions, numActions);
    return panels;
}

void initPanels_Mobile(iWidget *panels, iWidget *parentWidget, 
                       const iMenuItem *itemsNullTerminated,
                       const iMenuItem *actions, size_t numActions) {
    /* A multipanel widget has a top panel and one or more detail panels. In a horizontal layout,
       the detail panels slide in from the right and cover the top panel. In a landscape layout,
       the detail panels are always visible on the side. */
    setBackgroundColor_Widget(panels, uiBackground_ColorId);
    setFlags_Widget(panels,
                    resizeToParentWidth_WidgetFlag | resizeToParentHeight_WidgetFlag |
                        frameless_WidgetFlag | focusRoot_WidgetFlag | commandOnClick_WidgetFlag |
                        horizontalOffset_WidgetFlag |
                        /*overflowScrollable_WidgetFlag |*/ leftEdgeDraggable_WidgetFlag,
                    iTrue);
    panels->flags2 |= fadeBackground_WidgetFlag2;
    setFlags_Widget(panels, overflowScrollable_WidgetFlag, iFalse);
    /* The top-level split between main and detail panels. */
    iWidget *mainDetailSplit = makeHDiv_Widget(); {
        setCommandHandler_Widget(mainDetailSplit, mainDetailSplitHandler_);
        setFlags_Widget(mainDetailSplit, resizeHeightOfChildren_WidgetFlag, iFalse);
        setId_Widget(mainDetailSplit, "mdsplit");
        addChild_Widget(panels, iClob(mainDetailSplit));
    }
    /* The panel roots. */
    iWidget *topPanel = new_Widget(); {
        setId_Widget(topPanel, "panel.top");
        setDrawBufferEnabled_Widget(topPanel, iTrue);
        setCommandHandler_Widget(topPanel, topPanelHandler_);
        setFlags_Widget(topPanel,
                        arrangeVertical_WidgetFlag | resizeWidthOfChildren_WidgetFlag |
                            arrangeHeight_WidgetFlag | overflowScrollable_WidgetFlag |
                            commandOnClick_WidgetFlag,
                        iTrue);
        addChild_Widget(mainDetailSplit, iClob(topPanel));
        setId_Widget(addChild_Widget(topPanel, iClob(makePadding_Widget(0))), "panel.toppad");
    }
    iWidget *detailStack = new_Widget(); {
        setId_Widget(detailStack, "detailstack");
        setFlags_Widget(detailStack, collapse_WidgetFlag | resizeWidthOfChildren_WidgetFlag, iTrue);
        addChild_Widget(mainDetailSplit, iClob(detailStack));
    }
    /* Slide top panel with detail panels. */ {
        setFlags_Widget(topPanel, refChildrenOffset_WidgetFlag, iTrue);
        topPanel->offsetRef = detailStack;
    }
    /* Navigation bar at the top. */
    iLabelWidget *naviBack;
    iWidget *navi = new_Widget(); {
        setId_Widget(navi, "panel.navi");
        setBackgroundColor_Widget(navi, uiBackground_ColorId);
        setId_Widget(addChildFlags_Widget(navi,
                                          iClob(new_LabelWidget("", NULL)),
                                          alignLeft_WidgetFlag | fixedPosition_WidgetFlag |
                                              fixedSize_WidgetFlag | hidden_WidgetFlag |
                                              frameless_WidgetFlag),
                     "detailtitle");
        naviBack = addChildFlags_Widget(
            navi,
            iClob(newKeyMods_LabelWidget(
                leftAngle_Icon " ${panel.back}", SDLK_ESCAPE, 0, "panel.close")),
            noBackground_WidgetFlag | frameless_WidgetFlag | alignLeft_WidgetFlag |
                extraPadding_WidgetFlag);
        checkIcon_LabelWidget(naviBack);
        setId_Widget(as_Widget(naviBack), "panel.back");
        setFont_LabelWidget(naviBack, labelFont_());
        addChildFlags_Widget(panels, iClob(navi),
                             drawBackgroundToVerticalSafeArea_WidgetFlag |
                                 arrangeHeight_WidgetFlag | resizeWidthOfChildren_WidgetFlag |
                                 resizeToParentWidth_WidgetFlag | arrangeVertical_WidgetFlag);        
    }
    iBool haveDetailPanels = iFalse;
    /* Create panel contents based on provided items. */
    for (size_t i = 0; itemsNullTerminated[i].label; i++) {
        const iMenuItem *item = &itemsNullTerminated[i];
        if (equal_Command(item->label, "panel")) {
            haveDetailPanels = iTrue;
            const char *id = cstr_Command(item->label, "id");
            const iString *label = hasLabel_Command(item->label, "text")
                                       ? collect_String(suffix_Command(item->label, "text"))
                                       : collectNewFormat_String("${%s}", id);
            iLabelWidget * button =
                addChildFlags_Widget(topPanel,
                                     iClob(makePanelButton_(cstr_String(label), "panel.open")),
                                     borderTop_WidgetFlag);
            setChevron_LabelWidget(button, iTrue);
            const iChar icon = toInt_String(string_Command(item->label, "icon"));
            if (icon) {
                setIcon_LabelWidget(button, icon);
            }
            iWidget *panel = addChildPanel_(detailStack, button, NULL);
            if (argLabel_Command(item->label, "noscroll")) {
                setFlags_Widget(panel, overflowScrollable_WidgetFlag, iFalse);
            }
            makePanelItems_Mobile(panel, item->data);
        }
        else {
            makePanelItem_Mobile(topPanel, item);
        }
    }
    /* Actions. */
    if (numActions) {
        /* Some actions go in the navigation bar and some go on the top panel. */
        const iMenuItem *cancelItem = findDialogCancelAction_(actions, numActions);
        const iMenuItem *defaultItem = &actions[numActions - 1];
        iAssert(defaultItem);
        if (defaultItem && !cancelItem) {
            updateTextCStr_LabelWidget(naviBack, defaultItem->label);
            setCommand_LabelWidget(naviBack, collectNewCStr_String(defaultItem->command));
            setFlags_Widget(as_Widget(naviBack), alignLeft_WidgetFlag, iFalse);
            setFlags_Widget(as_Widget(naviBack), alignRight_WidgetFlag, iTrue);
            setIcon_LabelWidget(naviBack, 0);
            setFont_LabelWidget(naviBack, labelBoldFont_());            
        }
        else if (defaultItem && defaultItem != cancelItem) {
            if (!haveDetailPanels) {
                updateTextCStr_LabelWidget(naviBack, cancelItem->label);
                setCommand_LabelWidget(naviBack, collectNewCStr_String(cancelItem->command
                                                                       ? cancelItem->command
                                                                       : "cancel"));
            }
            iLabelWidget *defaultButton = new_LabelWidget(defaultItem->label, defaultItem->command);
            setFont_LabelWidget(defaultButton, labelBoldFont_());
            setFlags_Widget(as_Widget(defaultButton),
                            frameless_WidgetFlag | extraPadding_WidgetFlag |
                                noBackground_WidgetFlag,
                            iTrue);
            addChildFlags_Widget(as_Widget(naviBack), iClob(defaultButton),
                                 moveToParentRightEdge_WidgetFlag);
            updateSize_LabelWidget(defaultButton);
        }
        /* All other actions are added as buttons. */
        iBool needPadding = iTrue;
        for (size_t i = 0; i < numActions; i++) {
            const iMenuItem *act = &actions[i];
            if (act == cancelItem || act == defaultItem) {
                continue;
            }
            const char *label = act->label;
            if (*label == '*' || *label == '&') {
                continue; /* Special value selection items for a Question dialog. */
            }
            if (!iCmpStr(label, "---")) {
                continue; /* Separator. */
            }
            if (needPadding) {
                makePanelItem_Mobile(topPanel, &(iMenuItem){ "padding" });
                needPadding = iFalse;
            }
            makePanelItem_Mobile(
                topPanel,
                &(iMenuItem){ format_CStr("button text:" uiTextAction_ColorEscape "%s", act->label),
                              0,
                              0,
                              act->command });
        }
    }
    /* Finalize the layout. */
    if (parentWidget) {
        addChild_Widget(parentWidget, iClob(panels));
    }
    mainDetailSplitHandler_(mainDetailSplit, "window.resized"); /* make it resize the split */
    updatePanelSheetMetrics_(panels);
    arrange_Widget(panels);
    postCommand_App("widget.overflow"); /* with the correct dimensions */    
//    printTree_Widget(panels);
}

/*
         Landscape Layout                 Portrait Layout
                                      
┌─────────┬──────Detail─Stack─────┐    ┌─────────┬ ─ ─ ─ ─ ┐
│         │┌───────────────────┐  │    │         │Detail
│         ││┌──────────────────┴┐ │    │         │Stack    │
│         │││┌──────────────────┴┐│    │         │┌──────┐
│         ││││                   ││    │         ││┌─────┴┐│
│         ││││                   ││    │         │││      │
│Top Panel││││                   ││    │Top Panel│││      ││
│         ││││      Panels       ││    │         │││Panels│
│         ││││                   ││    │         │││      ││
│         │└┤│                   ││    │         │││      │
│         │ └┤                   ││    │         │└┤      ││
│         │  └───────────────────┘│    │         │ └──────┘
└─────────┴───────────────────────┘    └─────────┴ ─ ─ ─ ─ ┘
                                                  underneath
 
In portrait, top panel and detail stack are all stacked together.
*/

void setupMenuTransition_Mobile(iWidget *sheet, iBool isIncoming) {
    if (!isUsingPanelLayout_Mobile()) {
        return;    
    }
    const iBool isHorizPanel = (flags_Widget(sheet) & horizontalOffset_WidgetFlag) != 0;
    if (isHorizPanel && isLandscape_App()) {
        return;
    }
    const int maxOffset = isHorizPanel            ? width_Widget(sheet)
                          : isPortraitPhone_App() ? height_Widget(sheet)
                                                  : (6 * gap_UI);
    if (isIncoming) {
        setVisualOffset_Widget(sheet, maxOffset, 0, 0);
        setVisualOffset_Widget(sheet, 0, 330, easeOut_AnimFlag | softer_AnimFlag);
    }
    else {
        const iBool wasDragged = iAbs(value_Anim(&sheet->visualOffset) - 0) > 1;
        setVisualOffset_Widget(sheet,
                               maxOffset,
                               wasDragged ? 100 : 200,
                               wasDragged ? 0 : easeIn_AnimFlag | softer_AnimFlag);
    }
}

void setupSheetTransition_Mobile(iWidget *sheet, int flags) {
    const iBool isIncoming = (flags & incoming_TransitionFlag) != 0;
    const int   dir        = flags & dirMask_TransitionFlag;
    if (!isUsingPanelLayout_Mobile()) {
        if (prefs_App()->uiAnimations) {
            setFlags_Widget(sheet, horizontalOffset_WidgetFlag, iFalse);
            if (isIncoming) {
                setVisualOffset_Widget(sheet, -height_Widget(sheet), 0, 0);
                setVisualOffset_Widget(sheet, 0, 200, easeOut_AnimFlag | softer_AnimFlag);
            }
            else {
                setVisualOffset_Widget(sheet, -height_Widget(sheet), 200, easeIn_AnimFlag);
            }
        }
        return;
    }
    if (isSideBySideLayout_()) {
        /* TODO: Landscape transitions? */
        return;
    }
    setFlags_Widget(sheet,
                    horizontalOffset_WidgetFlag,
                    dir == right_TransitionDir || dir == left_TransitionDir);
    if (isIncoming) {
        switch (dir) {
            case right_TransitionDir:
                setVisualOffset_Widget(sheet, size_Root(sheet->root).x, 0, 0);
                break;
            case left_TransitionDir:
                setVisualOffset_Widget(sheet, -size_Root(sheet->root).x, 0, 0);
                break;
            case top_TransitionDir:
                setVisualOffset_Widget(
                    sheet, -bottom_Rect(boundsWithoutVisualOffset_Widget(sheet)), 0, 0);
                break;
            case bottom_TransitionDir:
                setVisualOffset_Widget(sheet, height_Widget(sheet), 0, 0);
                break;
        }
        setVisualOffset_Widget(sheet, 0, 200, easeOut_AnimFlag);
    }        
    else {
        switch (dir) {
            case right_TransitionDir: {
                const iBool wasDragged = iAbs(value_Anim(&sheet->visualOffset)) > 0;
                setVisualOffset_Widget(sheet, size_Root(sheet->root).x, wasDragged ? 100 : 200,
                                       wasDragged ? 0 : easeIn_AnimFlag);
                break;
            }
            case left_TransitionDir:
                setVisualOffset_Widget(sheet, -size_Root(sheet->root).x, 200, easeIn_AnimFlag);
                break;
            case top_TransitionDir:
                setVisualOffset_Widget(sheet,
                                       -bottom_Rect(boundsWithoutVisualOffset_Widget(sheet)),
                                       200,
                                       easeIn_AnimFlag);
                break;
            case bottom_TransitionDir:
                setVisualOffset_Widget(sheet, height_Widget(sheet), 200, easeIn_AnimFlag);
                break;
        }
    }
}

int bottomSafeInset_Mobile(void) {
#if defined (iPlatformAppleMobile)
    float bot;
    safeAreaInsets_iOS(NULL, NULL, NULL, &bot);
    return iRound(bot);
#else
    return 0;
#endif
}
