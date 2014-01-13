Setup
-----

### Alias configuration ###

Site definitions are stored in aliases.drushrc.php file:

```php
<?php

$aliases['groups-dev'] = array(
  'root' => '/Users/martonkiss/Sites/00',
  'uri' => 'groups-dev.local',
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
$aliases["groups-dev"] = array (
  'root' => '/Users/martonkiss/Sites/00',
  'uri' => 'groups-dev.local',
  '#file' => '/Users/martonkiss/Workspace/openstack/drupalsitedeploy/drush/includes/../aliases.drushrc.php',
  '#name' => 'groups-dev',
);
```

### Initialize a new site environment ###

```bash
$ drush dsd-init @groups-dev http://static.openstack.org/groups/groups-dev-20140101-001.tar.gz
```

