<?php

/**
 * @file
 * User session handling functions.
 *
 * The user-level session storage handlers:
 * - _drupal_session_open()
 * - _drupal_session_close()
 * - _drupal_session_read()
 * - _drupal_session_write()
 * - _drupal_session_destroy()
 * - _drupal_session_garbage_collection()
 * are assigned by session_set_save_handler() in bootstrap.inc and are called
 * automatically by PHP. These functions should not be called directly. Session
 * data should instead be accessed via the $_SESSION superglobal.
 */

// Make sure apdqc_escape_string() is available.
require_once 'apdqc.mysql.inc';

if (!function_exists('_drupal_session_open')) {

  /**
   * Session handler assigned by session_set_save_handler().
   *
   * This function is used to handle any initialization, such as file paths or
   * database connections, that is needed before accessing session data. Drupal
   * does not need to initialize anything in this function.
   *
   * This function should not be called directly.
   *
   * @return bool
   *   This function will always return TRUE.
   */
  function _drupal_session_open() {
    return TRUE;
  }

  /**
   * Session handler assigned by session_set_save_handler().
   *
   * This function is used to close the current session. Because Drupal stores
   * session data in the database immediately on write, this function does
   * not need to do anything.
   *
   * This function should not be called directly.
   *
   * @return bool
   *   This function will always return TRUE.
   */
  function _drupal_session_close() {
    return TRUE;
  }

  /**
   * Reads an entire session from the database (internal use only).
   *
   * Also initializes the $user object for the user associated with the session.
   * This function is registered with session_set_save_handler() to support
   * database-backed sessions. It is called on every page load when PHP sets
   * up the $_SESSION superglobal.
   *
   * This function is an internal function and must not be called directly.
   * Doing so may result in logging out the current user, corrupting session
   * data or other unexpected behavior. Session data must always be accessed via
   * the $_SESSION superglobal.
   *
   * @param string $sid
   *   The session ID of the session to retrieve.
   *
   * @return string
   *   The user's session, or an empty string if no session exists.
   */
  function _drupal_session_read($sid) {
    global $user, $is_https;

    // Write and Close handlers are called after destructing objects
    // since PHP 5.0.5.
    // Thus destructors can use sessions but session handler can't use objects.
    // So we are moving session closure before destructing objects.
    drupal_register_shutdown_function('session_write_close');

    // Handle the case of first time visitors and clients that don't store
    // cookies (eg. web crawlers).
    $insecure_session_name = substr(session_name(), 1);
    if (empty($sid) || (!isset($_COOKIE[session_name()]) && !isset($_COOKIE[$insecure_session_name]))) {
      $user = drupal_anonymous_user();
      return '';
    }

    // Otherwise, if the session is still active, we have a record of the
    // client's session in the database. If it's HTTPS then we are either have
    // a HTTPS session or we are about to log in so we check the sessions table
    // for an anonymous session with the non-HTTPS-only cookie.
    if ($is_https) {
      $query = \Database::getConnection()->prefixTables("
        SELECT
          GROUP_CONCAT(CONCAT(r.rid, '|||', r.name) separator ';;;') AS rid_name,
          u.*,
          s.*
        FROM {users} AS u
        INNER JOIN {sessions} AS s
          ON u.uid = s.uid
          AND s.ssid = '" . apdqc_escape_string($sid) . "'
        LEFT JOIN {users_roles} AS ur
          ON ur.uid = u.uid
        LEFT JOIN {role} AS r
          ON ur.rid = r.rid
        GROUP BY s.ssid
      ");

      // Run Query.
      $results = apdqc_query(array('users', 'sessions'), array($sid), $query);
      if ($results === 'NO DB') {
        // Core connection fallback.
        $user = db_query("SELECT u.*, s.* FROM {users} u INNER JOIN {sessions} s ON u.uid = s.uid WHERE s.ssid = :ssid", array(':ssid' => $sid))->fetchObject();
      }
      elseif ($results instanceof mysqli_result) {
        $user = $results->fetch_object();
      }
      else {
        $user = NULL;
      }

      if (!$user && isset($_COOKIE[$insecure_session_name])) {
        $query = \Database::getConnection()->prefixTables("
          SELECT
            GROUP_CONCAT(CONCAT(r.rid, '|||', r.name) separator ';;;') AS rid_name,
            u.*,
            s.*
          FROM {users} u
          INNER JOIN {sessions} s
            ON u.uid = s.uid
            AND s.sid = '" . apdqc_escape_string($_COOKIE[$insecure_session_name]) . "'
            AND s.uid = 0
          LEFT JOIN {users_roles} AS ur
            ON ur.uid = u.uid
          LEFT JOIN {role} AS r
            ON ur.rid = r.rid
          GROUP BY s.ssid
        ");
        // Run Query.
        $results = apdqc_query(array('users', 'sessions'), array($_COOKIE[$insecure_session_name]), $query);
        if ($results === 'NO DB') {
          // Core connection fallback.
          $user = db_query("SELECT u.*, s.* FROM {users} u INNER JOIN {sessions} s ON u.uid = s.uid WHERE s.sid = :sid AND s.uid = 0", array(
            ':sid' => $_COOKIE[$insecure_session_name],
          ))->fetchObject();
        }
        elseif ($results instanceof mysqli_result) {
          $user = $results->fetch_object();
        }
        else {
          $user = NULL;
        }
      }
    }
    else {
      $query = \Database::getConnection()->prefixTables("
        SELECT
          GROUP_CONCAT(CONCAT(r.rid, '|||', r.name) separator ';;;') AS rid_name,
          u.*,
          s.*
        FROM {users} u
        INNER JOIN {sessions} s
          ON u.uid = s.uid
          AND s.sid = '" . apdqc_escape_string($sid) . "'
        LEFT JOIN {users_roles} AS ur
          ON ur.uid = u.uid
        LEFT JOIN {role} AS r
          ON ur.rid = r.rid
        GROUP BY s.ssid
      ");
      // Run Query.
      $results = apdqc_query(array('users', 'sessions'), array($sid), $query);
      if ($results === 'NO DB') {
        $user = db_query("SELECT u.*, s.* FROM {users} u INNER JOIN {sessions} s ON u.uid = s.uid WHERE s.sid = :sid", array(':sid' => $sid))->fetchObject();
      }
      elseif ($results instanceof mysqli_result) {
        $user = $results->fetch_object();
      }
      else {
        $user = NULL;
      }
    }

    // Decode rid_name field.
    if ($user && !empty($user->rid_name)) {
      $roles = array();
      if ($user->uid > 0 && $user->status == 1) {
        $roles[DRUPAL_AUTHENTICATED_RID] = 'authenticated user';
        $groups = explode(';;;', $user->rid_name);
        foreach ($groups as $group) {
          $rid_name = explode('|||', $group);
          $roles[$rid_name[0]] = $rid_name[1];
        }
      }
      $user->roles = $roles;
      unset($user->rid_name);
    }

    // We found the client's session record and they are an authenticated,
    // active user.
    if ($user && $user->uid > 0 && $user->status == 1) {
      if (!isset($user->roles)) {
        // This is done to unserialize the data member of $user.
        $data = @unserialize($user->data);
        if ($user->data === 'b:0;' || $data !== FALSE) {
          $user->data = $data;
        }

        // Add roles element to $user.
        $user->roles = array();
        $user->roles[DRUPAL_AUTHENTICATED_RID] = 'authenticated user';

        $query = \Database::getConnection()->prefixTables("
          SELECT
            r.rid,
            r.name
          FROM {role} r
          INNER JOIN {users_roles} ur
            ON ur.rid = r.rid
          WHERE ur.uid = '" . apdqc_escape_string($user->uid) . "'
        ");
        // Run Query.
        $roles = array();
        $results = apdqc_query(array('role', 'users_roles'), array($user->uid), $query);
        if ($results === 'NO DB') {
          $roles = db_query("SELECT r.rid, r.name FROM {role} r INNER JOIN {users_roles} ur ON ur.rid = r.rid WHERE ur.uid = :uid", array(':uid' => $user->uid))->fetchAllKeyed(0, 1);
        }
        elseif ($results instanceof mysqli_result) {
          while ($row = $results->fetch_assoc()) {
            $roles[$row['rid']] = $row['name'];
          }
        }
        $user->roles += $roles;
      }
    }
    elseif ($user) {
      // The user is anonymous or blocked. Only preserve two fields from the
      // {sessions} table.
      $account = drupal_anonymous_user();
      $account->session = $user->session;
      $account->timestamp = $user->timestamp;
      $user = $account;
    }
    else {
      // The session has expired.
      $user = drupal_anonymous_user();
      $user->session = '';
    }

    // Store the session that was read for comparison in
    // _drupal_session_write().
    $last_read = &drupal_static('drupal_session_last_read');
    $last_read = array(
      'sid' => $sid,
      'value' => $user->session,
    );

    return $user->session;
  }

  /**
   * Writes an entire session to the database (internal use only).
   *
   * This function is registered with session_set_save_handler() to support
   * database-backed sessions.
   *
   * This function is an internal function and must not be called directly.
   * Doing so may result in corrupted session data or other unexpected behavior.
   * Session data must always be accessed via the $_SESSION superglobal.
   *
   * @param string $sid
   *   The session ID of the session to write to.
   * @param string $value
   *   Session data to write as a serialized string.
   *
   * @return bool
   *   Always returns TRUE.
   */
  function _drupal_session_write($sid, $value) {
    global $user, $is_https;

    // The exception handler is not active at this point, so we need to do it
    // manually.
    try {
      if (!drupal_save_session()) {
        // We don't have anything to do if we are not allowed to save the
        // session.
        return TRUE;
      }

      // Check whether $_SESSION has been changed in this request.
      $last_read = &drupal_static('drupal_session_last_read');
      $is_changed = !isset($last_read) || $last_read['sid'] != $sid || $last_read['value'] !== $value;

      // For performance reasons, do not update the sessions table, unless
      // $_SESSION has changed or more than 180 has passed since the last
      // update.
      if ($is_changed || !isset($user->timestamp) || REQUEST_TIME - $user->timestamp > variable_get('session_write_interval', 180)) {
        // Either ssid or sid or both will be added from $key below.
        $fields = array(
          'uid' => $user->uid,
          'cache' => isset($user->cache) ? $user->cache : 0,
          'hostname' => ip_address(),
          'session' => $value,
          'timestamp' => REQUEST_TIME,
        );

        // Use the session ID as 'sid' and an empty string as 'ssid' by default.
        // _drupal_session_read() does not allow empty strings so that's a safe
        // default.
        $key = array('sid' => $sid, 'ssid' => '');
        // On HTTPS connections, use the session ID as both 'sid' and 'ssid'.
        if ($is_https) {
          $key['ssid'] = $sid;
          // The "secure pages" setting allows a site to simultaneously use both
          // secure and insecure session cookies. If enabled and both cookies
          // are presented then use both keys.
          if (variable_get('https', FALSE)) {
            $insecure_session_name = substr(session_name(), 1);
            if (isset($_COOKIE[$insecure_session_name])) {
              $key['sid'] = $_COOKIE[$insecure_session_name];
            }
          }
        }
        elseif (variable_get('https', FALSE)) {
          unset($key['ssid']);
        }

        if (!isset($key['ssid'])) {
          $key['ssid'] = '';
        }

        $query = \Database::getConnection()->prefixTables("INSERT INTO {sessions}");
        $query .= " (sid, ssid, uid, cache, hostname, session, timestamp) VALUES
          ( '" . apdqc_escape_string($key['sid']) . "',
            '" . apdqc_escape_string($key['ssid']) . "',
            '" . apdqc_escape_string($fields['uid']) . "',
            '" . apdqc_escape_string($fields['cache']) . "',
            '" . apdqc_escape_string($fields['hostname']) . "',
            '" . apdqc_escape_string($fields['session']) . "',
            '" . apdqc_escape_string($fields['timestamp']) . "')";
        $query .= " ON DUPLICATE KEY UPDATE
          uid='" . apdqc_escape_string($fields['uid']) . "',
          cache='" . apdqc_escape_string($fields['cache']) . "',
          hostname='" . apdqc_escape_string($fields['hostname']) . "',
          session='" . apdqc_escape_string($fields['session']) . "',
          timestamp='" . apdqc_escape_string($fields['timestamp']) . "'";
        // Run Query.
        $results = apdqc_query(array('sessions'), $key, $query, array('async' => TRUE));
        if ($results === 'NO DB') {
          db_merge('sessions')
            ->key($key)
            ->fields($fields)
            ->execute();
        }
      }

      // Likewise, do not update access time more than once per 180 seconds.
      if ($user->uid && REQUEST_TIME - $user->access > variable_get('session_write_interval', 180)) {
        $query = \Database::getConnection()->prefixTables("UPDATE {users}");
        $query .= " SET access='" . apdqc_escape_string(REQUEST_TIME) . "'
          WHERE uid = '" . apdqc_escape_string($user->uid) . "'
        ";
        // Run Query.
        $results = apdqc_query(array('users'), array($user->uid), $query, array('async' => TRUE));
        if ($results === 'NO DB') {
          db_update('users')
            ->fields(array(
              'access' => REQUEST_TIME,
            ))
            ->condition('uid', $user->uid)
            ->execute();
        }
      }

      return TRUE;
    }
    catch (Exception $exception) {
      require_once DRUPAL_ROOT . '/includes/errors.inc';
      // If we are displaying errors, then do so with no possibility of a
      // further uncaught exception being thrown.
      if (error_displayable()) {
        print '<h1>Uncaught exception thrown in session handler.</h1>';
        print '<p>' . _drupal_render_exception_safe($exception) . '</p><hr />';
      }
      return FALSE;
    }
  }

  /**
   * Initializes the session handler, starting a session if needed.
   */
  function drupal_session_initialize() {
    global $user, $is_https;

    session_set_save_handler('_drupal_session_open', '_drupal_session_close', '_drupal_session_read', '_drupal_session_write', '_drupal_session_destroy', '_drupal_session_garbage_collection');

    // We use !empty() in the following check to ensure that blank session IDs
    // are not valid.
    if (!empty($_COOKIE[session_name()]) || ($is_https && variable_get('https', FALSE) && !empty($_COOKIE[substr(session_name(), 1)]))) {
      // If a session cookie exists, initialize the session. Otherwise the
      // session is only started on demand in drupal_session_commit(), making
      // anonymous users not use a session cookie unless something is stored in
      // $_SESSION. This allows HTTP proxies to cache anonymous pageviews.
      drupal_session_start();
      if (!empty($user->uid) || !empty($_SESSION)) {
        drupal_page_is_cacheable(FALSE);
      }
    }
    else {
      // Set a session identifier for this request. This is necessary because
      // we lazily start sessions at the end of this request, and some
      // processes (like drupal_get_token()) needs to know the future
      // session ID in advance.
      $GLOBALS['lazy_session'] = TRUE;
      $user = drupal_anonymous_user();
      // Less random sessions (which are much faster to generate) are used for
      // anonymous users than are generated in drupal_session_regenerate() when
      // a user becomes authenticated.
      session_id(drupal_random_key());
      if ($is_https && variable_get('https', FALSE)) {
        $insecure_session_name = substr(session_name(), 1);
        $session_id = drupal_random_key();
        $_COOKIE[$insecure_session_name] = $session_id;
      }
    }
    date_default_timezone_set(drupal_get_user_timezone());
    return TRUE;
  }

  /**
   * Starts a session forcefully, preserving already set session data.
   *
   * @ingroup php_wrappers
   */
  function drupal_session_start() {
    // Command line clients do not support cookies nor sessions.
    // Also do not send headers if they have already been sent out.
    if (!headers_sent() && !drupal_session_started() && !drupal_is_cli()) {
      // Save current session data before starting it, as PHP will destroy it.
      $session_data = isset($_SESSION) ? $_SESSION : NULL;

      session_start();
      drupal_session_started(TRUE);

      // Restore session data.
      if (!empty($session_data)) {
        $_SESSION += $session_data;
      }
    }
    return TRUE;
  }

  /**
   * Commits the current session, if necessary.
   *
   * If an anonymous user already have an empty session, destroy it.
   */
  function drupal_session_commit() {
    global $user, $is_https;

    if (!drupal_save_session()) {
      // We don't have anything to do if we are not allowed to save the session.
      return TRUE;
    }

    if (empty($user->uid) && empty($_SESSION)) {
      // There is no session data to store, destroy the session if it was
      // previously started.
      if (drupal_session_started()) {
        session_destroy();
      }
    }
    else {
      // There is session data to store. Start the session if it is not already
      // started.
      if (!drupal_session_started()) {
        drupal_session_start();
        if ($is_https && variable_get('https', FALSE)) {
          $insecure_session_name = substr(session_name(), 1);
          $params = session_get_cookie_params();
          $expire = $params['lifetime'] ? REQUEST_TIME + $params['lifetime'] : 0;
          setcookie($insecure_session_name, $_COOKIE[$insecure_session_name], $expire, $params['path'], $params['domain'], FALSE, $params['httponly']);
        }
      }
      // Write the session data.
      session_write_close();
    }
    return TRUE;
  }

  /**
   * Returns whether a session has been started.
   */
  function drupal_session_started($set = NULL) {
    static $session_started = FALSE;
    if (isset($set)) {
      $session_started = $set;
    }
    return $session_started && session_id();
  }

  /**
   * Called when an anonymous user becomes authenticated or vice-versa.
   *
   * @ingroup php_wrappers
   */
  function drupal_session_regenerate() {
    global $user, $is_https;
    // Nothing to do if we are not allowed to change the session.
    if (!drupal_save_session()) {
      return TRUE;
    }

    if ($is_https && variable_get('https', FALSE)) {
      $insecure_session_name = substr(session_name(), 1);
      if (!isset($GLOBALS['lazy_session']) && isset($_COOKIE[$insecure_session_name])) {
        $old_insecure_session_id = $_COOKIE[$insecure_session_name];
      }
      $params = session_get_cookie_params();
      $session_id = drupal_random_key();
      // If a session cookie lifetime is set, the session will expire
      // $params['lifetime'] seconds from the current request. If it is not set,
      // it will expire when the browser is closed.
      $expire = $params['lifetime'] ? REQUEST_TIME + $params['lifetime'] : 0;
      setcookie($insecure_session_name, $session_id, $expire, $params['path'], $params['domain'], FALSE, $params['httponly']);
      $_COOKIE[$insecure_session_name] = $session_id;
    }

    if (drupal_session_started()) {
      $old_session_id = session_id();
      // PHP 7.3 requires that we close session before setting new session ID.
      $original_session_saving = drupal_save_session();
      drupal_save_session(FALSE);
      session_write_close();
      drupal_session_started(FALSE);
    }
    session_id(drupal_random_key());

    if (isset($old_session_id)) {
      // Preserve and restore user object, as starting session will reset it.
      $original_user = $user;
      drupal_session_start();
      $user = $original_user;
      drupal_save_session($original_session_saving);
      $params = session_get_cookie_params();
      $expire = $params['lifetime'] ? REQUEST_TIME + $params['lifetime'] : 0;
      setcookie(session_name(), session_id(), $expire, $params['path'], $params['domain'], $params['secure'], $params['httponly']);
      $fields = array('sid' => session_id());
      if ($is_https) {
        $fields['ssid'] = session_id();
        // If the "secure pages" setting is enabled, use the newly-created
        // insecure session identifier as the regenerated sid.
        if (variable_get('https', FALSE)) {
          $fields['sid'] = $session_id;
        }
      }
      $query = \Database::getConnection()->prefixTables("UPDATE {sessions}");
      $query .= " SET sid = '" . apdqc_escape_string($fields['sid']) . "'";
      if (isset($fields['ssid'])) {
        $query .= ", ssid = '" . apdqc_escape_string($fields['ssid']) . "'";
      }
      $query .= " WHERE " . ($is_https ? 'ssid' : 'sid') . " = '" . apdqc_escape_string($old_session_id) . "'";
      // Run Query.
      $results = apdqc_query(array('sessions'), array($old_session_id), $query, array('async' => TRUE));
      if ($results === 'NO DB') {
        db_update('sessions')
          ->fields($fields)
          ->condition($is_https ? 'ssid' : 'sid', $old_session_id)
          ->execute();
      }
    }
    elseif (isset($old_insecure_session_id)) {
      // If logging in to the secure site, and there was no active session on
      // the secure site but a session was active on the insecure site, update
      // the insecure session with the new session identifiers.
      $query = \Database::getConnection()->prefixTables("UPDATE {sessions}");
      $query .= " SET sid = '" . apdqc_escape_string($session_id) . "',";
      $query .= " ssid = '" . apdqc_escape_string(session_id()) . "'";
      $query .= " WHERE sid = '" . apdqc_escape_string($old_insecure_session_id) . "'";
      // Run Query.
      $results = apdqc_query(array('sessions'), array($old_session_id), $query, array('async' => TRUE));
      if ($results === 'NO DB') {
        db_update('sessions')
          ->fields(array('sid' => $session_id, 'ssid' => session_id()))
          ->condition('sid', $old_insecure_session_id)
          ->execute();
      }
    }
    else {
      // Start the session when it doesn't exist yet.
      // Preserve the logged in user, as it will be reset to anonymous
      // by _drupal_session_read.
      $account = $user;
      drupal_session_start();
      $user = $account;
    }
    date_default_timezone_set(drupal_get_user_timezone());
    return TRUE;
  }

  /**
   * Session handler assigned by session_set_save_handler().
   *
   * Cleans up a specific session.
   *
   * @param string $sid
   *   Session ID.
   */
  function _drupal_session_destroy($sid) {
    global $user, $is_https;

    // Nothing to do if we are not allowed to change the session.
    if (!drupal_save_session()) {
      return TRUE;
    }

    // Delete session data.
    $query = \Database::getConnection()->prefixTables("DELETE FROM {sessions}");
    $query .= " WHERE " . ($is_https ? 'ssid' : 'sid') . " = '" . apdqc_escape_string($sid) . "'";
    // Run Query.
    $results = apdqc_query(array('sessions'), array($sid), $query, array('async' => TRUE));
    if ($results === 'NO DB') {
      db_delete('sessions')
        ->condition($is_https ? 'ssid' : 'sid', $sid)
        ->execute();
    }

    // Reset $_SESSION and $user to prevent a new session from being started
    // in drupal_session_commit().
    $_SESSION = array();
    $user = drupal_anonymous_user();

    // Unset the session cookies.
    _drupal_session_delete_cookie(session_name());
    if ($is_https) {
      _drupal_session_delete_cookie(substr(session_name(), 1), FALSE);
    }
    elseif (variable_get('https', FALSE)) {
      _drupal_session_delete_cookie('S' . session_name(), TRUE);
    }
    return TRUE;
  }

  /**
   * Deletes the session cookie.
   *
   * @param string $name
   *   Name of session cookie to delete.
   * @param bool $secure
   *   Force the secure value of the cookie.
   */
  function _drupal_session_delete_cookie($name, $secure = NULL) {
    global $is_https;
    if (isset($_COOKIE[$name]) || (!$is_https && $secure === TRUE)) {
      $params = session_get_cookie_params();
      if ($secure !== NULL) {
        $params['secure'] = $secure;
      }
      setcookie($name, '', REQUEST_TIME - 3600, $params['path'], $params['domain'], $params['secure'], $params['httponly']);
      unset($_COOKIE[$name]);
    }
    return TRUE;
  }

  /**
   * Ends a specific user's session(s).
   *
   * @param int $uid
   *   User ID.
   */
  function drupal_session_destroy_uid($uid) {
    // Nothing to do if we are not allowed to change the session.
    if (!drupal_save_session()) {
      return TRUE;
    }

    $query = \Database::getConnection()->prefixTables("DELETE FROM {sessions}");
    $query .= " WHERE uid = '" . apdqc_escape_string($uid) . "'";
    // Run Query.
    $results = apdqc_query(array('sessions'), array($uid), $query, array('async' => TRUE));
    if ($results === 'NO DB') {
      db_delete('sessions')
        ->condition('uid', $uid)
        ->execute();
    }
  }

  /**
   * Session handler assigned by session_set_save_handler().
   *
   * Cleans up stalled sessions.
   *
   * @param int $lifetime
   *   The value of session.gc_maxlifetime, passed by PHP.
   *   Sessions not updated for more than $lifetime seconds will be removed.
   */
  function _drupal_session_garbage_collection($lifetime) {
    // Be sure to adjust 'php_value session.gc_maxlifetime' to a large enough
    // value. For example, if you want user sessions to stay in your database
    // for three weeks before deleting them, you need to set gc_maxlifetime
    // to '1814400'. At that value, only after a user doesn't log in after
    // three weeks (1814400 seconds) will his/her session be removed.
    $query = \Database::getConnection()->prefixTables("DELETE FROM {sessions}");
    $query .= " WHERE timestamp < '" . apdqc_escape_string(REQUEST_TIME - $lifetime) . "'";
    // Run Query.
    $results = apdqc_query(array('sessions'), array('*'), $query, array('async' => TRUE));
    if ($results === 'NO DB') {
      db_delete('sessions')
        ->condition('timestamp', REQUEST_TIME - $lifetime, '<')
        ->execute();
    }
    return TRUE;
  }

  /**
   * Determines whether to save session data of the current request.
   *
   * This function allows the caller to temporarily disable writing of
   * session data, should the request end while performing potentially
   * dangerous operations, such as manipulating the global $user object.
   * See http://drupal.org/node/218104 for usage.
   *
   * @param bool $status
   *   Disables writing of session data when FALSE, (re-)enables
   *   writing when TRUE.
   *
   * @return bool
   *   FALSE if writing session data has been disabled. Otherwise, TRUE.
   */
  function drupal_save_session($status = NULL) {
    // PHP session ID, session, and cookie handling happens in the global scope.
    // This value has to persist across calls to drupal_static_reset(), since a
    // potentially wrong or disallowed session would be written otherwise.
    static $save_session = TRUE;
    if (isset($status)) {
      $save_session = $status;
    }
    return $save_session;
  }

}
