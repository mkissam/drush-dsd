Setup
-----

### Alias configuration ###

Site definitions are stored in aliases.drushrc.php file:

```php
<?php

$aliases['groups-dev'] = array(
  'root' => '/Users/username/Sites/groups-dev/w',
  'dsd-root' => '/Users/username/Sites/groups-dev',
  'uri' => 'groups-dev.local',
  'databases' => array(
    'default' => array(
      'driver' => 'mysql',
      'username' => 'dsdtest',
      'password'  => 'my-s3cr3t-passw0rd',
      'port' => '',
      'host' => 'localhost',
      'database' => 'dsdtest',
    ),
  ),
  'file-owner' => 'username',
  'file-group' => 'www-data',
  'variables' => array(
    'site_name' => 'My Drupal Site',
  ),
  'profile' => 'standard',
  'default-admin-password' => 'my-s3cr3t-admin-passw0rd',
  'disable-features-revert' => TRUE,
   // Static tarball provider
  'package-provider' => 'static-tarball',
  'package-repository' => 'http://tarballs.local/groups',
  'package-branch' => 'groups-latest',
   // Drush dl manifest provider
  'package-provider' => 'drush-dl',
  'package-repository' => 'http://tarballs.openstack.org/groups/drupal-updates/release-history',
  'package-dist-name' => 'groups',
  'package-dev-branch' => TRUE,
);

```

Usage
-----

### List available site definitions ###

```bash
$ drush sa
groups-dev
none
```

### Display site definition details ###

```bash
$ drush sa @groups-dev
$aliases['groups-dev'] = array(
  'root' => '/Users/username/Sites/groups-dev/w',
  'dsd-root' => '/Users/username/Sites/groups-dev',
  'uri' => 'groups-dev.local',
  'databases' => array(
    'default' => array(
      'driver' => 'mysql',
      'username' => 'dsdtest',
      'password'  => 'my-s3cr3t-passw0rd',
      'port' => '',
      'host' => 'localhost',
      'database' => 'dsdtest',
    ),
  ),
  'file-owner' => 'username',
  'file-group' => 'www-data',
  'variables' => array(
    'site_name' => 'My Drupal Site',
  ),
  'profile' => 'standard',
  'default-admin-password' => 'my-s3cr3t-admin-passw0rd',
  'disable-features-revert' => TRUE,
  '#file' => '/Users/username/Workspace/openstack/drupalsitedeploy/drush/includes/../aliases.drushrc.php',
  '#name' => 'groups-dev',
);
```

### Initialize a new site environment ###

With tarball provider:

```bash
$ wget http://ftp.drupal.org/files/projects/drupal-7.24.tar.gz
$ drush dsd-init @groups-dev drupal-7.24.tar.gz
```

Easy way with drush-dl provider:

```bash
$ drush dsd-init @groups-dev
```

### Upgrade to a new release or snapshot ###

With tarball provider:

```bash
$ wget http://ftp.drupal.org/files/projects/drupal-7.25.tar.gz
$ drush dsd-update @groups-dev drupal-7.25.tar.gz
```

With drush-dl provider:

```bash
$ drush dsd-update @groups-dev
```

### Query available releases

```bash
$ drush dsd-status @groups-dev
```


### Rollback to latest working version ###

```bash
$ drush dsd-rollback @groups-dev
```

