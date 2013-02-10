/***************************************************************************
* Copyright (c) 2013 Reza Fatahilah Shah <rshah0385@kireihana.com>
*
* Permission is hereby granted, free of charge, to any person
* obtaining a copy of this software and associated documentation
* files (the "Software"), to deal in the Software without restriction,
* including without limitation the rights to use, copy, modify, merge,
* publish, distribute, sublicense, and/or sell copies of the Software,
* and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
* OR OTHER DEALINGS IN THE SOFTWARE.
*
***************************************************************************/
var docRoot = null;

function initializeMenu()
{
    // move the mouse area to the menu's parent element
    comboMouseArea.parent = comboRoot.parent;

    // move the contextual menu to the document's root
    comboRoot.parent = getDocRoot();
}

function getDocRoot()
{
    if(!docRoot)
    {
        docRoot = comboRoot.parent;

        while(docRoot.parent)
        {
            docRoot = docRoot.parent;
        }
    }
    return docRoot;
}

function showMenu(mouse)
{
    var map = comboMouseArea.mapFromItem(null, 0, 0)
    comboBorder.x = Math.abs(map.x)
    comboBorder.y = Math.abs(map.y) + comboMouseArea.height

    comboRoot.state = "visible";
    comboListView.forceActiveFocus();
}

function hideMenu()
{
    comboRoot.state = "";
}
