<?php
//
// ZoneMinder web logout view file, $Date: 2008-07-25 10:48:16 +0100 (Fri, 25 Jul 2008) $, $Revision: 2612 $
// Copyright (C) 2001-2008 Philip Coombes
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//

$focusWindow = true;

xhtmlHeaders(__FILE__, $SLANG['Logout'] );
?>
<body>
  <div id="page">
    <div id="header">
      <h1>ZoneMinder <?= $SLANG['Logout'] ?></h1>
    </div>
    <div id="content">
      <form name="contentForm" id="contentForm" method="post" action="<?= $_SERVER['PHP_SELF'] ?>">
        <input type="hidden" name="action" value="logout"/>
        <input type="hidden" name="view" value="login"/>
        <p><?= sprintf( $CLANG['CurrentLogin'], $user['Username'] ) ?></p>
        <p>
          <input type="submit" value="<?= $SLANG['Logout'] ?>"/>
<?php
if ( ZM_USER_SELF_EDIT )
{
?>
          <input type="button" value="<?= $SLANG['Config'] ?>" onclick="createPopup( '?view=user&uid=<?= $user['Id'] ?>', 'zmUser', 'user' );"/>
<?php
}
?>
          <input type="button" value="<?= $SLANG['Cancel'] ?>" onclick="closeWindow();"></p>
        </p>
      </form>
    </div>
  </div>
</body>
</html>
