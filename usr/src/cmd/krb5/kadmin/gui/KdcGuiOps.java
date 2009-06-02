/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * ident	"@(#)KdcGuiOps.java	1.2	05/06/08 SMI"
 *
 * Copyright (c) 2001 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

/**
 * This file was originally automatically generated by Java WorkShop.
 *
 * Runtime vendor: SunSoft, Inc.
 * Runtime version: 1.0
 *
 * Visual vendor: SunSoft, Inc.
 * Visual version: 1.0
 */


import sunsoft.jws.visual.rt.base.*;
import sunsoft.jws.visual.rt.type.*;
import java.awt.Event;


public class KdcGuiOps extends Operations {
    private Op ops[];
    
    private KdcGui group;
    private KdcGuiRoot gui;
    
    public void setGroup(Group group) {
        this.group = (KdcGui)group;
    }
    
    public void setRoot(Root root) {
        this.gui = (KdcGuiRoot)root;
        if (ops == null)
            initializeOps();
    }
    
    public boolean handleMessage(Message msg) {
        for (int i = 0; i < ops.length; i++) {
            if (ops[i].hasCode()) {
                if (ops[i].matchMessage(msg)) {
                    handleCallback(i, msg,
				   (msg.isAWT ? (Event)msg.arg : null));
                }
            } else {
                ops[i].handleMessage(msg);
            }
        }
        
        return false;
    }
    
    private void initializeOps() {
        ops = new Op[17];
        
        ops[0] = new Op(gui);
        ops[0].name = "Op1";
        ops[0].filter = new OpFilter();
        ops[0].filter.filterType = OpFilter.EVENT;
        ops[0].filter.target = gui.Exit2;
        ops[0].filter.id = 1001;
        ops[0].action = new OpAction();
        ops[0].action.actionType = OpAction.CODE;
        
        ops[1] = new Op(gui);
        ops[1].name = "Op1";
        ops[1].filter = new OpFilter();
        ops[1].filter.filterType = OpFilter.EVENT;
        ops[1].filter.target = gui.browserHelp1;
        ops[1].filter.id = 1001;
        ops[1].action = new OpAction();
        ops[1].action.actionType = OpAction.CODE;
        
        ops[2] = new Op(gui);
        ops[2].name = "Op1";
        ops[2].filter = new OpFilter();
        ops[2].filter.filterType = OpFilter.EVENT;
        ops[2].filter.target = gui.Context2;
        ops[2].filter.id = 1001;
        ops[2].action = new OpAction();
        ops[2].action.actionType = OpAction.CODE;
        
        ops[3] = new Op(gui);
        ops[3].name = "Op1";
        ops[3].filter = new OpFilter();
        ops[3].filter.filterType = OpFilter.EVENT;
        ops[3].filter.target = gui.About2;
        ops[3].filter.id = 1001;
        ops[3].action = new OpAction();
        ops[3].action.actionType = OpAction.CODE;
        
        ops[4] = new Op(gui);
        ops[4].name = "Exit";
        ops[4].filter = new OpFilter();
        ops[4].filter.filterType = OpFilter.EVENT;
        ops[4].filter.target = gui.mainframe;
        ops[4].filter.id = 1001;
        ops[4].action = new OpAction();
        ops[4].action.actionType = OpAction.CODE;
        
        ops[5] = new Op(gui);
        ops[5].name = "Op1";
        ops[5].filter = new OpFilter();
        ops[5].filter.filterType = OpFilter.EVENT;
        ops[5].filter.target = gui.PrintCurPr;
        ops[5].filter.id = 1001;
        ops[5].action = new OpAction();
        ops[5].action.actionType = OpAction.CODE;
        
        ops[6] = new Op(gui);
        ops[6].name = "Op1";
        ops[6].filter = new OpFilter();
        ops[6].filter.filterType = OpFilter.EVENT;
        ops[6].filter.target = gui.PrintCurPol;
        ops[6].filter.id = 1001;
        ops[6].action = new OpAction();
        ops[6].action.actionType = OpAction.CODE;
        
        ops[7] = new Op(gui);
        ops[7].name = "Op1";
        ops[7].filter = new OpFilter();
        ops[7].filter.filterType = OpFilter.EVENT;
        ops[7].filter.target = gui.PrintPrlist;
        ops[7].filter.id = 1001;
        ops[7].action = new OpAction();
        ops[7].action.actionType = OpAction.CODE;
        
        ops[8] = new Op(gui);
        ops[8].name = "Op1";
        ops[8].filter = new OpFilter();
        ops[8].filter.filterType = OpFilter.EVENT;
        ops[8].filter.target = gui.PrintPollist;
        ops[8].filter.id = 1001;
        ops[8].action = new OpAction();
        ops[8].action.actionType = OpAction.CODE;
        
        ops[9] = new Op(gui);
        ops[9].name = "Op1";
        ops[9].filter = new OpFilter();
        ops[9].filter.filterType = OpFilter.EVENT;
        ops[9].filter.target = gui.logout;
        ops[9].filter.id = 1001;
        ops[9].action = new OpAction();
        ops[9].action.actionType = OpAction.CODE;
        
        ops[10] = new Op(gui);
        ops[10].name = "Exit";
        ops[10].filter = new OpFilter();
        ops[10].filter.filterType = OpFilter.EVENT;
        ops[10].filter.target = gui.Exit;
        ops[10].filter.id = 1001;
        ops[10].action = new OpAction();
        ops[10].action.actionType = OpAction.CODE;
        
        ops[11] = new Op(gui);
        ops[11].name = "Op1";
        ops[11].filter = new OpFilter();
        ops[11].filter.filterType = OpFilter.EVENT;
        ops[11].filter.target = gui.editPreferences;
        ops[11].filter.id = 1001;
        ops[11].action = new OpAction();
        ops[11].action.actionType = OpAction.CODE;
        
        ops[12] = new Op(gui);
        ops[12].name = "Op1";
        ops[12].filter = new OpFilter();
        ops[12].filter.filterType = OpFilter.EVENT;
        ops[12].filter.target = gui.refreshPrincipals;
        ops[12].filter.id = 1001;
        ops[12].action = new OpAction();
        ops[12].action.actionType = OpAction.CODE;
        
        ops[13] = new Op(gui);
        ops[13].name = "Op1";
        ops[13].filter = new OpFilter();
        ops[13].filter.filterType = OpFilter.EVENT;
        ops[13].filter.target = gui.refreshPolicies;
        ops[13].filter.id = 1001;
        ops[13].action = new OpAction();
        ops[13].action.actionType = OpAction.CODE;
        
        ops[14] = new Op(gui);
        ops[14].name = "Op1";
        ops[14].filter = new OpFilter();
        ops[14].filter.filterType = OpFilter.EVENT;
        ops[14].filter.target = gui.browserHelp2;
        ops[14].filter.id = 1001;
        ops[14].action = new OpAction();
        ops[14].action.actionType = OpAction.CODE;
        
        ops[15] = new Op(gui);
        ops[15].name = "Op1";
        ops[15].filter = new OpFilter();
        ops[15].filter.filterType = OpFilter.EVENT;
        ops[15].filter.target = gui.Context;
        ops[15].filter.id = 1001;
        ops[15].action = new OpAction();
        ops[15].action.actionType = OpAction.CODE;
        
        ops[16] = new Op(gui);
        ops[16].name = "Op1";
        ops[16].filter = new OpFilter();
        ops[16].filter.filterType = OpFilter.EVENT;
        ops[16].filter.target = gui.About;
        ops[16].filter.id = 1001;
        ops[16].action = new OpAction();
        ops[16].action.actionType = OpAction.CODE;
    }
    
    private void handleCallback(int index, Message msg, Event evt) {
        switch (index) {
	case 0:
            {
                group.checkExit(
				(java.awt.Frame)gui.loginframe.getBody());
            }
            break;
	case 1:
            {
                group.checkHelp(
				(java.awt.Frame)gui.loginframe.getBody());
            }
            break;
	case 2:
            {
                group.checkContextSensitiveHelp(
				(java.awt.Frame)gui.loginframe.getBody());
            }
            break;
	case 3:
            {
                group.checkAbout(
				 (java.awt.Frame)gui.loginframe.getBody());
            }
            break;
	case 4:
            {
                group.exit();
            }
            break;
	case 5:
            {
                group.checkPrintCurPr();
            }
            break;
	case 6:
            {
                group.checkPrintCurPol();
            }
            break;
	case 7:
            {
                group.checkPrintPrList();
            }
            break;
	case 8:
            {
                group.checkPrintPoList();
            }
            break;
	case 9:
            {
                group.checkLogout();
            }
            break;
	case 10:
            {
                group.checkExit(
				(java.awt.Frame)gui.mainframe.getBody());
            }
            break;
	case 11:
            {
                group.checkEditPreferences();
            }
            break;
	case 12:
            {
                group.checkRefreshPrincipals();
            }
            break;
	case 13:
            {
                group.checkRefreshPolicies();
            }
            break;
	case 14:
            {
                group.checkHelp(
				(java.awt.Frame)gui.mainframe.getBody());
            }
            break;
	case 15:
            {
                group.checkContextSensitiveHelp(
				(java.awt.Frame)gui.mainframe.getBody());
            }
            break;
	case 16:
            {
                group.checkAbout(
				 (java.awt.Frame)gui.mainframe.getBody());
            }
            break;
	default:
            throw new Error("Bad callback index: " + index);
        }
    }
    
    
    // methods from lib/visual/gen/methods.java
    
    /**
     * Converts a string to the specified type.
     */
    private Object convert(String type, String value) {
        return (Converter.getConverter(type).convertFromString(value));
    }
}
