<?php
/**
 * @file
 * Drush Site Deploy commands.
 */

function dsd_drush_help($section) {
  switch ($section) {
    case 'meta:dsd:title':
      return dt('Drush site deploy commands');
    case 'meta:dsd:summary':
      return dt('Deploy and upgrade Drupal sites.');
  }
}

function dsd_drush_command() {
  $items['dsd-init'] = array(
    'description' => 'Initialize a site environment',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
    'arguments' => array(
      'site' => 'Site alias',
      'file' => 'The site archive file that contains the distribution.',
    ),
    'required-arguments' => 1,
  );
  $items['dsd-update'] = array(
      'description' => 'Update an existing site environment',
      'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
      'arguments' => array(
          'site' => 'Site alias',
          'file' => 'The site archive file that contains the distribution.',
      ),
      'required-arguments' => 1,
  );
  $items['dsd-rollback'] = array(
      'description' => 'Rollback to the previous site environment',
      'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
      'arguments' => array(
          'site' => 'Site alias',
      ),
      'required-arguments' => 1,
  );
  $items['dsd-status'] = array(
      'description' => 'Report deployment status',
      'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
      'arguments' => array(
          'site' => 'Site alias',
      ),
      'required-arguments' => 1,
  );
  return $items;
}

/**
 * Return a site alias from command arguments.
 *
 * @return Ambigous <Always, boolean>|mixed
 */

function _drush_dsd_get_sitealias() {
  $command = drush_get_command();
  $specifications = $command['arguments'][0];
  if (!empty($specifications)) {
    list($site_list, $not_found) = drush_sitealias_resolve_sitespecs($specifications);
    if (!empty($not_found)) {
      return drush_set_error('DRUSH_ALIAS_NOT_FOUND', dt("Not found: @list", array("@list" => implode(', ', $not_found))));
    }
    if (count($site_list) > 1) {
      return drush_set_error('DRUSH_ALIAS_TOO_MUCH', 'You need to specify only one site alias.');
    }
  } else {
    return drush_set_error('DRUSH_ALIAS_NOT_FOUND', 'You must specifiy a site alias.');
  }
  return reset($site_list);
}

/**
 * Execute drush site-install in a prepared Drupal environment
 *
 * @param unknown $site
 * @param unknown $subdirs
 * @return boolean
 */

function _drush_dsd_site_install($site, $subdirs) {
  drush_print('Site install.');
  $profile = isset($site['profile']) ? $site['profile'] : 'standard';
  $db = $site['databases']['default'];
  $db_url = $db['driver'].'://'.$db['username'].':'.$db['password'].'@'.$db['host'].'/'.$db['database'];
  $result = drush_shell_cd_and_exec($subdirs['slot0'], "drush site-install -y $profile --db-url=$db_url");
  if ($result != TRUE) {
    return drush_set_error('DRUSH_DSD_INSTALL_FAILED', 'Drush site-install failed.');
  }
  // set settings.php file permission to r/o
  chmod($subdirs['slot0'].'/sites/default/settings.php', 0440);
  return TRUE;
}

/**
 * Set site variables of an installed Drupal site
 *
 * @param unknown $site
 * @param unknown $subdirs
 */

function _drush_dsd_set_default_variables($site, $subdirs) {
  if (isset($site['variables'])) {
    drush_print('Set variables.');
    foreach ($site['variables'] as $key => $value) {
      drush_shell_cd_and_exec($subdirs['slot0'], "drush vset --yes $key '$value'");
    }
  }
}

/**
 * Set default admin password of an installed Drupal site.
 *
 * @param unknown $site
 * @param unknown $subdirs
 */

function _drush_dsd_set_admin_password($site, $subdirs) {
  if (isset($site['default-admin-password'])) {
    drush_print("Set default admin password.");
    $admin_pwd = $site['default-admin-password'];
    drush_shell_cd_and_exec($subdirs['slot0'], "drush upwd admin --password='$admin_pwd'");
  }
}

/**
 * Revert features after installation / update
 *
 * @param unknown $site
 * @param unknown $subdirs
 */

function _drush_dsd_features_revert($site, $subdirs, $slot) {
  // TODO: check whether features module is present and enabled in Drupal deployment.
  if (!isset($site['disable-features-revert'])) {
    drush_print("Revert features.");
    drush_shell_cd_and_exec($subdirs[$slot], "drush features-revert-all --yes");
  }
}

/**
 * Set active slot of a site deployment.
 *
 * @param unknown $slot
 * @param unknown $subdirs
 */

function _drush_dsd_set_active_slot($slot, $subdirs) {
  drush_print(dt('Activate @slot.', array('@slot' => $slot)));
  if (file_exists($subdirs['w'])) {
    unlink($subdirs['w']);
  }
  symlink($subdirs[$slot], $subdirs['w']);
}

/**
 * Return the name of active slot.
 *
 * @param unknown $subdirs
 */

function _drush_dsd_get_active_slot($subdirs) {
  $target = readlink($subdirs['w']);
  return basename($target);
}

/**
 * Return an associative array from subdirectories of a given siteroot.
 *
 * @param unknown $root
 */

function _drush_dsd_get_subdirectories($root) {
  $subdirs = array();
  foreach (array('etc', 'slot0', 'slot1', 'files', 'backup', 'release', 'w') as $k) {
    $subdirs[$k] = join(DIRECTORY_SEPARATOR, array($root, $k));
  }
  return $subdirs;
}

/**
 * Extract archive and move to target slot
 *
 * @param unknown $subdirs
 * @param unknown $archive_path
 * @param unknown $slot
 * @return Ambigous <Always, boolean>|boolean
 */

function _drush_dsd_extract_archive($subdirs, $site, $archive_path, $slot) {
  drush_log('Extract archive', 'ok');
  $tmp = drush_tempdir();
  $tar = drush_get_tar_executable();
  umask(0027);
  if (drush_shell_cd_and_exec($tmp, "$tar --no-same-permissions -xzf ". $archive_path) != TRUE) {
    return drush_set_error('DRUSH_DSD_EXTRACT_FAIL', 'Failed to extract archive file.');
  }
  $tmp_dirs = drush_scan_directory($tmp, '/./', array('.', '..', 'CVS'), 0, FALSE);
  if (count($tmp_dirs) != 1) {
    return drush_set_error('DRUSH_DSD_EXTRACT_FAIL', 'Invalid archive structure, single directory excepted in archive root.');
  }
  umask(0022);
  $dist_temp = reset($tmp_dirs);
  if (!is_dir($dist_temp->filename)) {
    return drush_set_error('DRUSH_DSD_EXTRACT_FAIL', 'No root directory was found in archive.');
  }

  $user = $site['file-owner'];
  $group = $site['file-group'];
  drush_shell_cd_and_exec($tmp, "chown -R $user:$group ".$dist_temp->filename);

  // move directory to slot0
  drush_move_dir($dist_temp->filename, $subdirs[$slot], TRUE);

  return TRUE;
}

/**
 * Link the common shared directory to a selected slot.
 *
 * @param unknown $subdirs
 * @param unknown $slot
 */

function _drush_dsd_link_files($subdirs, $slot) {
  symlink($subdirs['files'], join(DIRECTORY_SEPARATOR, array($subdirs[$slot], 'sites', 'default', 'files')));
}

/**
 * Set maintenance mode for a given slot.
 *
 * @param unknown $subdirs
 * @param unknown $slot
 * @param string $enable
 */

function _drush_dsd_set_maintenance_mode($subdirs, $slot, $enable = TRUE) {
  if ($enable) {
    drush_shell_cd_and_exec($subdirs[$slot], "drush vset -y maintenance_mode 1");
  } else {
    drush_shell_cd_and_exec($subdirs[$slot], "drush vset -y maintenance_mode 0");
  }
}

/**
 * Backup the slot's database into backup dir
 *
 * @param unknown $subdirs
 * @param unknown $site
 * @param unknown $slot
 */

function _drush_dsd_backup_database($subdirs, $site, $slot) {
  // TODO: garbage collection of old backup files
  // TODO: compress backup file
  // NOTICE: pgsql could ask for password * requires testing *

  // generate backup filename
  $backup_filename = join(DIRECTORY_SEPARATOR, array($subdirs['backup'], $site['#name'].'.sql.gz'));
  drush_shell_cd_and_exec($subdirs[$slot], "drush sql-dump | gzip > $backup_filename");
}

/**
 * Restore database from a previous backup
 *
 * @param unknown $subdirs
 * @param unknown $site
 * @param unknown $slot
 */

function _drush_dsd_restore_database($subdirs, $site, $slot) {
  $backup_filename = join(DIRECTORY_SEPARATOR, array($subdirs['backup'], $site['#name'].'.sql.gz'));
  if (!file_exists($backup_filename)) {
    return drush_set_error('DRUSH_DSD_DB_NOT_FOUND', dt('No database archive was found: @backup_filename.', array('@backup_filename' => $backup_filename)));
  }
  if (!drush_shell_cd_and_exec($subdirs[$slot], "gzcat $backup_filename | drush sqlc")) {
    return drush_set_error('DRUSH_DSD_DB_RESTORE_FAIL', 'Failed to restore database archive');
  }

  return TRUE;
}

function _drush_dsd_updatedb($subdirs, $site, $slot) {
  drush_shell_cd_and_exec($subdirs[$slot], "drush -y updatedb");
}

function _drush_dsd_cache_clear($subdirs, $site, $slot) {
  drush_shell_cd_and_exec($subdirs[$slot], "drush cc all");
}

/**
 * Implementation of drush dsd-init command.
 * Initialize a new site environment and preinstall a drupal distribution.
 *
 * @return boolean|Ambigous <Always, boolean>
 */

function drush_dsd_init() {
  drush_include_engine('release_info', 'updatexml');

  $site = _drush_dsd_get_sitealias();
  if ($site == FALSE) {
    return FALSE;
  }

  // set site defaults
  if (!isset($site['file-group'])) {
    $site['file-group'] = 'www-data';
  }
  if (!isset($site['file-owner'])) {
    $userInfo = posix_getpwuid(posix_getuid());
    $site['file-owner'] = $userInfo['name'];
  }

  $command = drush_get_command();
  if ($site['package-provider'] == 'static-tarball') {
    $archive_filename = $command['arguments'][1];
    if (!file_exists($archive_filename)) {
      return drush_set_error('DRUSH_DSD_ARCHIVE_NOT_FOUND', dt('Missing archive file: @file', array('@file' => $archive_filename)));
    }
  }

  // create directory structure
  drush_print('Create directory structure.');
  if (!is_dir($site['dsd-root'])) {
    return drush_set_error('DRUSH_DSD_ROOT_DIR_NOT_FOUND', dt("Not found dsd-root directory: @dir", array('@dir', $site['root'])));
  }
  $subdirs = _drush_dsd_get_subdirectories($site['dsd-root']);

  // check slot0
  if (is_dir($subdirs['slot0'])) {
    return drush_set_error('DRUSH_DSD_EXISTS', 'slot0 exists, abort deployment.');
  }

  // create every directory except slot0
  foreach (array('slot1', 'files', 'backup', 'release') as $k) {
    drush_mkdir($subdirs[$k]);
    chmod($subdirs[$k], 0750);
    chown($subdirs[$k], $site['file-owner']);
    chgrp($subdirs[$k], $site['file-group']);
  }
  chmod($subdirs['files'], 02770); // g+rwxs
  //chgrp($subdirs['files'], $site['file-group']);

  switch ($site['package-provider']) {
    case 'static-tarball':
      drush_print('Extract archive.');
      if (strpos($archive_filename, '/') === 0) {
        $archive_path = $archive_filename;
      } else if (strpos($archive_filename, 'http://' === 0)) {
        // TODO: implement remote url fetch
        die('Remote url fetch not implemented.');
      } else {
        // make path relative to current directory
        $archive_path = join(DIRECTORY_SEPARATOR, array(realpath('.'), $archive_filename));
      }

      // extract archive file
      _drush_dsd_extract_archive($subdirs, $site, $archive_path, 'slot0');

      // move md5 file to release info file when exists
      $md5filename = str_replace('.tar.gz', '.md5', $archive_path);
      drush_print('md5filename: '.$md5filename);
      if (file_exists($md5filename)) {
        drush_op('copy', $md5filename ,$subdirs['slot0'] . '/release');
      }
      break;
    case 'drush-dl':

      $request = array(
        'name' => $site['package-dist-name'],
        'version' => '',
        'drupal_version' => '7.x',
        'project_version' => '',
      );

      if (isset($site['package-repository'])) {
        $request['status url'] = $site['package-repository'];
      }
      $dev = ($site['package-dev-branch'] == TRUE);
      $restrict_to = $dev ? 'dev' : '';
      $release_info = release_info_fetch($request, $restrict_to);

      $stmt = 'drush dl';
      $stmt .= ' --debug';
      if (isset($site['package-repository'])) {
        $stmt .= ' --source='.$site['package-repository'];
      }
      if ($dev) {
        $stmt .= ' --dev';
      }
      $stmt .= ' --drupal-project-rename=slot0';
      $stmt .= ' '.$site['package-dist-name'];
      umask(0027);
      $result = drush_shell_cd_and_exec($site['dsd-root'], $stmt);
      umask(0022);
      if ($result != TRUE) {
        return drush_set_error('DRUSH_DSD_INSTALL_FAILED', 'Distribution download failed.');
      }
      $user = $site['file-owner'];
      $group = $site['file-group'];
      drush_shell_cd_and_exec($site['dsd-root'], "chown -R $user:$group slot0");
      $ri = sprintf('%s %s', $release_info['name'], $release_info['mdhash']);
      file_put_contents($subdirs['slot0'] . '/release', $ri);
      break;
  }

  // clone settings.php and set permission as writeable
  $source_settings = $subdirs['slot0'] . '/sites/default/default.settings.php';
  $target_settings = $subdirs['slot0'] . '/sites/default/settings.php';
  // inject custom settings feature to default.settings.php
  $local_settings_src = "
// Override settings in this file.
if (file_exists('./' . conf_path() . '/local_settings.php')) {
  include_once './' . conf_path() . '/local_settings.php';
}
";
  file_put_contents($source_settings, $local_settings_src, FILE_APPEND);
  drush_op('copy', $source_settings, $target_settings);
  chmod($target_settings, 0664);
  chgrp($target_settings, $site['file-group']);

  // clone local_settings
  $source_local = $subdirs['etc'] . '/settings.php';
  $target_local = $subdirs['slot0'] . '/sites/default/local_settings.php';
  if (file_exists($source_local)) {
    drush_op('copy', $source_local, $target_local);
    chmod($target_local, 0440);
    chgrp($target_local, $site['file-group']);
  }

  _drush_dsd_link_files($subdirs, 'slot0');

  if (!_drush_dsd_site_install($site, $subdirs)) {
    return FALSE;
  }
  _drush_dsd_set_default_variables($site, $subdirs);
  _drush_dsd_set_admin_password($site, $subdirs);
  _drush_dsd_features_revert($site, $subdirs, 'slot0');
  _drush_dsd_set_active_slot('slot0', $subdirs);

}

/**
 * Implementation of drush dsd-update command.
 * Update an existing drupal site deployment.
 *
 */

function drush_dsd_update() {
  if (($site = _drush_dsd_get_sitealias()) == FALSE) {
    return FALSE;
  }
  $subdirs = _drush_dsd_get_subdirectories($site['dsd-root']);

  // determine slots
  $active_slot = _drush_dsd_get_active_slot($subdirs);
  drush_log(dt('Active slot: @slot', array('@slot'  => $active_slot)), 'ok');
  $update_slot = $active_slot == 'slot0' ? 'slot1' : 'slot0';

  // validate parameters
  $command = drush_get_command();
  switch ($site['package-provider']) {
    case 'static-tarball':
      $archive_filename = $command['arguments'][1];
      if (!file_exists($archive_filename)) {
        return drush_set_error('DRUSH_DSD_ARCHIVE_NOT_FOUND', dt('Missing archive file: @file', array('@file' => $archive_filename)));
      }

      if (strpos($archive_filename, '/') === 0) {
        $archive_path = $archive_filename;
      } else if (strpos($archive_filename, 'http://' === 0)) {
        // TODO: implement remote url fetch
        die('Remote url fetch not implemented.');
      } else {
        // make path relative to current directory
        $archive_path = join(DIRECTORY_SEPARATOR, array(realpath('.'), $archive_filename));
      }

      // extract archive and move to target
      if (!_drush_dsd_extract_archive($subdirs, $site, $archive_path, $update_slot)) {
        return FALSE;
      }

      // move md5 file to release info file when exists
      $md5filename = str_replace('.tar.gz', '.md5', $archive_path);
      drush_print('md5filename: '.$md5filename);
      if (file_exists($md5filename)) {
        drush_op('copy', $md5filename ,$subdirs[$update_slot] . '/release');
      }
      break;
    case 'drush-dl':
      drush_include_engine('release_info', 'updatexml');
      $request = array(
        'name' => $site['package-dist-name'],
        'version' => '',
        'drupal_version' => '7.x',
        'project_version' => '',
      );

      if (isset($site['package-repository'])) {
        $request['status url'] = $site['package-repository'];
      }
      $dev = ($site['package-dev-branch'] == TRUE);
      $restrict_to = $dev ? 'dev' : '';
      $release_info = release_info_fetch($request, $restrict_to);

      $stmt = 'drush dl';
      $stmt .= ' --yes'; // pass --yes to override existing directories
      $stmt .= ' --debug';
      if (isset($site['package-repository'])) {
        $stmt .= ' --source='.$site['package-repository'];
      }
      if ($dev) {
        $stmt .= ' --dev';
      }
      $stmt .= ' --drupal-project-rename='.$update_slot;
      $stmt .= ' '.$site['package-dist-name'];
      umask(0027);
      $result = drush_shell_cd_and_exec($site['dsd-root'], $stmt);
      umask(0022);
      if ($result != TRUE) {
        return drush_set_error('DRUSH_DSD_INSTALL_FAILED', 'Distribution download failed.');
      }
      $user = $site['file-owner'];
      $group = $site['file-group'];
      drush_shell_cd_and_exec($site['dsd-root'], "chown -R $user:$group slot0");
      $ri = sprintf('%s %s', $release_info['name'], $release_info['mdhash']);
      file_put_contents($subdirs[$update_slot] . '/release', $ri);
      break;
  }

  // copy settings file from old slot and optionally merge custom
  // settings
  $source_settings = $subdirs[$active_slot] . '/sites/default/settings.php';
  $target_settings = $subdirs[$update_slot] . '/sites/default/settings.php';
  drush_op('copy', $source_settings, $target_settings);
  chmod($target_settings, 0440);
  chgrp($target_settings, $site['file-group']);

  // clone local_settings
  $source_local = $subdirs['etc'] . '/settings.php';
  $target_local = $subdirs[$update_slot] . '/sites/default/local_settings.php';
  if (file_exists($source_local)) {
    drush_op('copy', $source_local, $target_local);
    chmod($target_local, 0440);
    chgrp($target_local, $site['file-group']);
  }

  _drush_dsd_link_files($subdirs, $update_slot);
  _drush_dsd_set_maintenance_mode($subdirs, $active_slot);
  _drush_dsd_backup_database($subdirs, $site, $active_slot);
  _drush_dsd_updatedb($subdirs, $site, $update_slot);
  _drush_dsd_features_revert($site, $subdirs, $update_slot);
  _drush_dsd_cache_clear($subdirs, $site, $update_slot);
  _drush_dsd_set_active_slot($update_slot, $subdirs);
  _drush_dsd_set_maintenance_mode($subdirs, $active_slot, FALSE);

}

/**
 * Implementation of dsd-rollback command.
 * Revert actual site to latest working version.
 *
 * @return boolean
 */

function drush_dsd_rollback() {
  if (($site = _drush_dsd_get_sitealias()) == FALSE) {
    return FALSE;
  }
  $subdirs = _drush_dsd_get_subdirectories($site['dsd-root']);

  // determine slots
  $active_slot = _drush_dsd_get_active_slot($subdirs);
  drush_log(dt('Active slot: @slot', array('@slot'  => $active_slot)), 'ok');
  $revert_slot = $active_slot == 'slot0' ? 'slot1' : 'slot0';

  _drush_dsd_set_maintenance_mode($subdirs, $active_slot);
  if (!_drush_dsd_restore_database($subdirs, $site, $revert_slot)) {
    return FALSE;
  }
  _drush_dsd_set_active_slot($revert_slot, $subdirs);
  _drush_dsd_set_maintenance_mode($subdirs, $revert_slot, FALSE);
}

/**
 * Return release info file name.
 *
 */

function drush_dsd_get_release_info_filename($root, $slot) {
  return join(DIRECTORY_SEPARATOR, array($root, $slot, 'release'));
}

/**
 * Return release information for a given slot
 *
 * @param string $root
 *    site root directory
 * @param string $slot
 *    site slot
 * @return
 *   Content of slot info file, if not found returns
 *   N/A.
 *
 */

function drush_dsd_get_release_info($root, $slot) {
  $release_file = drush_dsd_get_release_info_filename($root, $slot);
  if (file_exists($release_file)) {
    // split md5
    return reset(explode(' ', file_get_contents($release_file)));
  } else {
    return "N/A";
  }
}

/**
 * Implementation of dsd-status command.
 * Report of actual deployment status of the site.
 *
 */

function drush_dsd_status() {
  if (($site = _drush_dsd_get_sitealias()) == FALSE) {
    return FALSE;
  }
  $subdirs = _drush_dsd_get_subdirectories($site['dsd-root']);
  $slot = '';
  $status = 'NOT INSTALLED';

  if (file_exists($site['dsd-root'].'/w')) {
    $active_slot = _drush_dsd_get_active_slot($subdirs);
  }

  if (file_exists($site['dsd-root'].'/w/index.php')) {
    $status = 'INSTALLED';

    // check available updates
    if (isset($site['package-provider'])) {
      switch ($site['package-provider']) {
        case 'static-tarball':
          $catalog = file_get_contents($site['package-repository'].'/'.
            $site['package-branch'].'.md5');
          $md5sum = reset(explode(' ', $catalog));
          $release_info = drush_dsd_get_release_info($site['dsd-root'], $active_slot);
          if ($md5sum != $release_info) {
            $status = 'UPDATE AVAILABLE';
          }
          $update = dt('(@localrelease => @newrelease)', array(
            '@localrelease' => $release_info,
            '@newrelease' => $md5sum));
          break;
        case 'drush-dl':
          drush_include_engine('release_info', 'updatexml');
          $request = array(
            'name' => $site['package-dist-name'],
            'version' => '',
            'drupal_version' => '7.x',
            'project_version' => '',
          );

          if (isset($site['package-repository'])) {
            $request['status url'] = $site['package-repository'];
          }
          $dev = ($site['package-dev-branch'] == TRUE);
          $restrict_to = $dev ? 'dev' : '';
          $release_info = release_info_fetch($request, $restrict_to);
          $ri = sprintf('%s %s', $release_info['name'], $release_info['mdhash']);
          $filename = drush_dsd_get_release_info_filename($site['dsd-root'], $active_slot);
          $release_info = file_get_contents($filename);
          if ($ri != $release_info) {
            $status = 'UPDATE AVAILABLE';
          }
          $update = dt('(@localrelease => @newrelease)', array(
            '@localrelease' => $release_info,
            '@newrelease' => $ri));
          break;
      }
    }
  }

  drush_print(dt('@status @slot @update', array(
    '@status' => $status,
    '@slot' => $slot,
    '@update' => $update)));
}