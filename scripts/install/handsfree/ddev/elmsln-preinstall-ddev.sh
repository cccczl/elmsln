#!/bin/bash
# preinstall is all the steps listed in INSTALL.txt but attemps to automate
# as many of them as possible. This will try and establish elmsln
# on the host system, then walk through all the directory operations,
# file system changes, example repo checkout and forking, confg.cfg
# creation, as well as hooking up crontab and the optimizations to
# apc,php,mysql and apache that can help make ELMSLN rock solid.
# It also tries to figure out what OS its installed on and then apply
# changes appropriate to that system. At the very least this can better
# inform you as to what its doing where

# where am i? move to where I am. This ensures source is properly sourced
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR

# provide messaging colors for output to console
txtbld=$(tput bold)             # Bold
bldgrn=${txtbld}$(tput setaf 2) #  green
bldred=${txtbld}$(tput setaf 1) #  red
txtreset=$(tput sgr0)
installlog="/var/www/elmsln/config/tmp/INSTALL-LOG.txt"
steplog="/var/www/elmsln/config/tmp/STEP-LOG.txt"
elmslnecho(){
  echo "${bldgrn}$1${txtreset}"
  echo "$1" >> $installlog
}
elmslnwarn(){
  echo "${bldred}$1${txtreset}"
  echo "$1" >> $installlog
}
# Define seconds timestamp
timestamp(){
  date +"%s"
}
# generate a uuid
getuuid(){
  uuidgen -rt
}
# check that we are the root user
if [[ $EUID -ne 0 ]]; then
  elmslnwarn "Please run as root"
  exit 1
fi
# we assume you install it in the place that we like
cd /var/www/elmsln

# blow away old repo
rm -rf config
# make git not track filemode changes
git config core.fileMode false
# ensure it doesn't track filemode changes in the future
git config --global core.filemode false

git clone https://github.com/elmsln/elmsln-config-example.git /var/www/elmsln/config
# set version to match the VERSION.txt we are starting with
cd /var/www/elmsln
code_version=$(<VERSION.txt)
cp /var/www/elmsln/VERSION.txt /var/www/elmsln/config/SYSTEM_VERSION.txt
cd /var/www/elmsln/config
touch /var/www/elmsln/config/logs/upgrade_history.txt
echo "Initially installed as: ${code_version}" >> /var/www/elmsln/config/logs/upgrade_history.txt
echo '0' > $steplog
rm -rf .git

# make a new repo
git init
# prevent filemode chnages on the new config directory
git config core.fileMode false
git add -A
git commit -m "initial ELMSLN config"

elmslnecho "Enter the git repo you want to keep config in sync with: (ex: {user}@{YOURPRIVATEREPO}:YOU/elmsln-config-YOURUNIT.git)"
read gitrepo
# ensure they type yes
if [ "$gitrepo" != "" ]; then
  elmslnecho "attempting to push current structure to the repo listed"
  git remote add origin $gitrepo
  git push origin master
else
  elmslnwarn "You did not enter a repo so this is in local version control but not tracked remotely. It is highly recommended that you hook this up to a remote repository in the future!"
fi

# detect what OS this is on and make suggestions for settings
cat /etc/*-release
dist="$(cat /etc/*-release)"

elmslnecho "The above should list information about the system this is being installed on. We currently support semi-automated install routines for RHEL, CentOS and Ubuntu. Please verify the above and select one of the following options:"
elmslnecho "1. RHEL 6.x / CentOS 6.x"
elmslnecho "2. Ubuntu / Debian"
elmslnecho "3. RHEL 7.x / CentOS 7.x"
elmslnecho "4. other / manual"
read os
if [ $os == '1' ]; then
  elmslnecho "treating this like a RHEL 6.x / CentOS 6.x install"
  wwwuser='apache'
  elmslnecho "www user automatically set to ${wwwuser}"
  # test for apcu which would mean we dont need to optimize apc
  if [ -f /etc/php.d/apcu.ini ]; then
    apcini=""
    apcuini="/etc/php.d/apcu.ini"
    elmslnecho "apcu.ini automatically set to ${apcuini}"
  else
    apcini="/etc/php.d/apc.ini"
    elmslnecho "apc.ini automatically set to ${apcini}"
  fi
  phpini="/etc/php.ini"
  elmslnecho "php.ini automatically set to ${phpini}"
  mycnf="/etc/my.cnf"
  elmslnecho "my.cnf automatically set to ${mycnf}"
  crontab="/var/spool/cron/"
  elmslnecho "crontab automatically set to ${crontab}"
  domains="/etc/httpd/conf.d/"
  elmslnecho "domains automatically set to ${domains}"
  zzz_performance="/etc/httpd/conf.d/zzz_performance.conf"
  elmslnecho "apache perforamnce tuning automatically set to ${zzz_performance}"
elif [[ $os == '2' && $dist == *"DISTRIB_RELEASE=18"* ]]; then
  elmslnecho "treating this like ubuntu 18"
  wwwuser='bto108a'
  elmslnecho "www user automatically set to ${wwwuser}"
  # test for apcu which would mean we dont need to optimize apc
  if [ -f /etc/php/7.2/mods-available/apcu.ini ]; then
    apcini=""
    apcuini="/etc/php/7.2/mods-available/apcu.ini"
    elmslnecho "apcu.ini automatically set to ${apcuini}"
  else
    apcini="/etc/php/7.2/mods-available/apc.ini"
    elmslnecho "apc.ini automatically set to ${apcini}"
  fi
  phpini="/etc/php/7.2/fpm/php.ini"
  elmslnecho "php.ini automatically set to ${phpini}"
  opcacheini="/etc/php/7.2/mods-available/opcache.ini"
  elmslnecho "opcache.ini automatically set to ${opcacheini}"
  mycnf="/etc/mysql/conf.d/mariadb_elmsln.cnf"
  elmslnecho "my.cnf automatically set to ${mycnf}"
  crontab="/var/spool/cron/crontabs/"
  elmslnecho "crontab automatically set to ${crontab}"
  domains="/etc/apache2/sites-available/"
  elmslnecho "domains automatically set to ${domains}"
  zzz_performance="/etc/apache2/conf-available/zzz_performance.conf"
  elmslnecho "apache perforamnce tuning automatically set to ${zzz_performance}"
elif [[ $os == '2' && $dist == *"DISTRIB_RELEASE=16"* ]]; then
  elmslnecho "treating this like ubuntu 16"
  wwwuser='bto108a'
  elmslnecho "www user automatically set to ${wwwuser}"
  # test for apcu which would mean we dont need to optimize apc
  if [ -f /etc/php/7.0/mods-available/apcu.ini ]; then
    apcini=""
    apcuini="/etc/php/7.0/mods-available/apcu.ini"
    elmslnecho "apcu.ini automatically set to ${apcuini}"
  else
    apcini="/etc/php/7.0/mods-available/apc.ini"
    elmslnecho "apc.ini automatically set to ${apcini}"
  fi
  phpini="/etc/php/7.0/fpm/php.ini"
  elmslnecho "php.ini automatically set to ${phpini}"
  opcacheini="/etc/php/7.0/mods-available/opcache.ini"
  elmslnecho "opcache.ini automatically set to ${opcacheini}"
  mycnf="/etc/mysql/conf.d/mariadb_elmsln.cnf"
  elmslnecho "my.cnf automatically set to ${mycnf}"
  crontab="/var/spool/cron/crontabs/"
  elmslnecho "crontab automatically set to ${crontab}"
  domains="/etc/apache2/sites-available/"
  elmslnecho "domains automatically set to ${domains}"
  zzz_performance="/etc/apache2/conf-available/zzz_performance.conf"
  elmslnecho "apache perforamnce tuning automatically set to ${zzz_performance}"
elif [[ $os == '2' ]]; then
  elmslnecho "treating this like ubuntu 14 or below"
  wwwuser='bto108a'
  elmslnecho "www user automatically set to ${wwwuser}"
  # test for apcu which would mean we dont need to optimize apc
  if [ -f /etc/php5/mods-available/apcu.ini ]; then
    apcini=""
    apcuini="/etc/php5/mods-available/apcu.ini"
    elmslnecho "apcu.ini automatically set to ${apcuini}"
  else
    apcini="/etc/php5/mods-available/apc.ini"
    elmslnecho "apc.ini automatically set to ${apcini}"
  fi
  phpini="/etc/php5/apache2/php.ini"
  phpfpmini="/etc/php5/fpm/php.ini"
  elmslnecho "php.ini automatically set to ${phpini}"
  mycnf="/etc/php5/mods-available/mysql.ini"
  elmslnecho "my.cnf automatically set to ${mycnf}"
  crontab="/var/spool/cron/crontabs/"
  elmslnecho "crontab automatically set to ${crontab}"
  domains="/etc/apache2/sites-available/"
  elmslnecho "domains automatically set to ${domains}"
  zzz_performance="/etc/apache2/conf-available/zzz_performance.conf"
  elmslnecho "apache perforamnce tuning automatically set to ${zzz_performance}"
elif [ $os == '3' ]; then
  elmslnecho "treating this like a RHEL 7.x / CentOS 7.x install"
  wwwuser='apache'
  elmslnecho "www user automatically set to ${wwwuser}"
  # file is low weighted when on the file system in Cen 7 land
  apcini=""
  apcuini="/etc/php.d/40-apcu.ini"
  elmslnecho "apcu.ini automatically set to ${apcuini}"
  phpini="/etc/php.ini"
  elmslnecho "php.ini automatically set to ${phpini}"
  # our install sets this up ahead of time
  mycnf="/etc/my.cnf"
  elmslnecho "my.cnf automatically set to ${mycnf}"
  crontab="/var/spool/cron/"
  elmslnecho "crontab automatically set to ${crontab}"
  mkdir -p /etc/httpd/conf.sites.d
  domains="/etc/httpd/conf.sites.d/"
  elmslnecho "domains automatically set to ${domains}"
  zzz_performance="/etc/httpd/conf.d/zzz_performance.conf"
  elmslnecho "apache perforamnce tuning automatically set to ${zzz_performance}"
else
  elmslnecho "need to ask you some more questions"
  # ask about apache
  elmslnecho "www user, what does apache run as? (www-data and apache are common)"
  read wwwuser

  elmslnecho "where is apc.ini? ex: /etc/php.d/apc.ini (empty to skip)"
  read apcini

  elmslnecho "where is php.ini? ex: /etc/php.ini (empty to skip)"
  read phpini

  elmslnecho "where is my.cnf? ex: /etc/my.cnf (empty to skip)"
  read mycnf

  elmslnecho "where is crontab? ex: /etc/crontab (empty to skip)"
  read crontab

  elmslnecho "where should elmsln apache conf elmsln.conf files live? ex: /etc/httpd/conf.d/ (empty to skip)"
  read domains

  elmslnecho "where should elmsln apache performance tweaks live? ex: /etc/httpd/conf.d/zzz_performance.conf (empty to skip)"
  read zzz_performance

  elmslnecho "Is this some flavor of linux like Ubuntu? (yes for travis, vagrant, etc)"
  read likeubuntu
  if [[ $likeubuntu == 'yes' ]]; then
    os='2'
  fi
fi

# based on where things commonly are. This would allow for non-interactive
# creation if the user approves of what has been found and allow us to
# do an automatic creation flag to establish this stuff without any input!

# work against the config file
config='/var/www/elmsln/config/scripts/drush-create-site/config.cfg'
configpwd='/var/www/elmsln/config/scripts/drush-create-site/configpwd.cfg'
touch $config
touch $configpwd
# step through creation of the config file
echo "#university / institution deploying this instance" >> $config
elmslnecho "what is your uniersity abbreviation? (ex psu)"
read university
echo "university='${university}'" >> $config

elmslnecho "two letter abbreviation for deployment? (ex aa for arts / architecture)"
read host
echo "host='${host}'" >> $config

elmslnecho "default email ending? (ex psu.edu)"
read emailending
echo "emailending='${emailending}'" >> $config

elmslnecho "base address for deployment? (ex aanda.psu.edu)"
read address
echo "address='${address}'" >> $config

elmslnecho "web service based address for deployment? (ex otherpath.psu.edu. this can be the same as the previous address if you want, this is typical)"
read serviceaddress
echo "serviceaddress='${serviceaddress}'" >> $config

elmslnecho "web service prefix? (if calls come from data-courses.otherpath.psu.edu then this would be 'data-' if you don't create domains this way then leave this blank)"
read serviceprefix
echo "serviceprefix='${serviceprefix}'" >> $config

elmslnecho "protocol for web traffic? (think long and hard before you type anything other then 'https'. there's a lot of crazy stuff out there so its better to encrypt everything.. EVERYTHING!)"
read protocol
echo "#protocol to use for front-facing calls" >> $config
echo "protocol='${protocol}'" >> $config
echo "#protocol to use for backend calls" >> $config
echo "dataprotocol='${protocol}'" >> $config
echo "#email that the site uses to send mail" >> $config
elmslnecho "site email address to use? (ex siteaddress@you.edu)"
read site_email
echo "site_email='${site_email}'" >> $config

echo "#administrator e-mail or alias" >> $config
elmslnecho "administrator e-mail or alias? (ex admin@you.edu)"
read admin
echo "admin='${admin}'" >> $config

# if there's a scary part it's right in here for some I'm sure
elmslnecho "database superuser credentials? (this is only stored in the configpwd.cfg file. it is recommended you create an alternate super user other then true root. user needs full permissions including grant since this is what requests new drupal sites to be produced)"
read -s dbsu
echo "dbsu='${dbsu}'" >> $configpwd
elmslnecho "database superuser password? (same notice as above)"
read -s dbsupw
echo "dbsupw='${dbsupw}'" >> $configpwd

# this was read in from above or automatically supplied based on the system type
echo "#www user, what does apache run as? www-data and apache are common" >> $config
echo "wwwuser='${wwwuser}'" >> $config

echo "#webgroup, usually admin if sharing with other developers else leave root" >> $config
elmslnecho "webgroup? (usually admin if sharing with other developers else leave root)"
read webgroup
echo "webgroup='bto108a'" >> $config

# append all these settings that we tell people NOT to modify
echo "" >> $config
echo "# uncomment the following if you are not using SSO" >> $config
echo "#send_requester_pw=yes" >> $config
echo "# where is elmsln installed, not recommended to move from here" >> $config
echo "elmsln='/var/www/elmsln'" >> $config
echo "" >> $config
echo "# these vars shouldn't need changing" >> $config
echo "webdir='/var/www/elmsln/domains'" >> $config
echo "# jobs location where job files write to" >> $config
echo "fileloc='/var/www/elmsln/config/jobs'" >> $config
echo "# hosts to allow split groups of elmsln based on college / group" >> $config
echo "hostfile='/var/www/elmsln/config/scripts/drush-create-site/hosts'" >> $config
echo "# compiled drupal \"stacks\"" >> $config
echo "stacks='/var/www/elmsln/core/dslmcode/stacks'" >> $config
echo "# location of drupal private files" >> $config
echo "drupal_priv='/var/www/elmsln/config/private_files'" >> $config
echo "drupal_tmp='/var/www/elmsln/config/tmp'" >> $config
echo "# configsdir" >> $config
echo "configsdir='/var/www/elmsln/config'" >> $config
# capture automatically generated values that can be used to reference this
# exact deployment of ELMSLN in the future
echo "" >> $config
echo "# ELMSLN INSTALLER GENERATED VALUES" >> $config
echo "# Do not modify below this line" >> $config
# capture install time; this could be used in the future similar to the
# drup timestamping to see if there are structural upgrade .sh commands needed
# contextually based on when we are installed. This will start to allow for
# hook_update_n style updates but at a bash / deployment level.
installed="$(timestamp)"
echo "elmsln_installed='${installed}'" >> $config
uuid="$(getuuid)"
# a uuid which data can be related on
echo "elmsln_uuid='${uuid}'" >> $config
# echo a uuid to a salt file we can use later on
touch /var/www/elmsln/config/SALT.txt
echo "$(getuuid)" > /var/www/elmsln/config/SALT.txt

# allow for opt in participation in our impact program
elmslnecho "Would you like to send anonymous usage statistics to http://elmsln.org for data visualization purposes? (type yes or anything else to opt out)"
read yesprompt
# ensure they type yes, this is a big deal command
if [[ $yesprompt == 'yes' ]]; then
  # include this instance in our statistics program
  echo "elmsln_stats_program='yes'" >> $config
else
  # we respect privacy even if it leads to less cool visualizations :)
  echo "elmsln_stats_program='no'" >> $config
fi
# performance / recommended settings
if [[ -n "$apcini" ]]; then
  rm $apcini
  cp /var/www/elmsln/scripts/server/apc.txt $apcini
fi
if [[ -n "$apcuini" ]]; then
  rm $apcuini
  cp /var/www/elmsln/scripts/server/apcu.txt $apcuini
fi
if [[ -n "$phpini" ]]; then
  cat /var/www/elmsln/scripts/server/php.txt >> $phpini
fi
if [[ -n "$phpfpmini" ]]; then
  cat /var/www/elmsln/scripts/server/php.txt >> $phpfpmini
fi
if [[ -n "$mycnf" ]]; then
  cat /var/www/elmsln/scripts/server/my.txt > $mycnf
fi
if [[ -n "$opcacheini" ]]; then
  cat /var/www/elmsln/scripts/server/opcache.txt >> $opcacheini
fi

# Add on our last bit of conf for our new Ubuntu Stuff.
if [[ $os == '2' && $dist == *"DISTRIB_RELEASE=20"* ]]; then
  cat /var/www/elmsln/scripts/server/my_ubunut20.txt >> $mycnf
fi

if [[ -n "$domains" ]]; then
  # try to automatically author the domains file(s)
  # @todo replace this part with the ability to split each domain off into its own conf file
  cp /var/www/elmsln/scripts/server/domains/* "$domains"
  # replace servicedomain partial with what was entered above
  sed -i "s/SERVICEYOURUNIT.edu/${serviceaddress}/g" ${domains}*.conf
  # replace domain partial with what was entered above
  sed -i "s/YOURUNIT.edu/${address}/g" ${domains}*.conf
  # replace servicedomain prefix if available with what was entered above
  sed -i "s/DATA./${serviceprefix}/g" ${domains}*.conf
  ls $domains
  elmslnecho "${domains} was automatically generated but you may want to verify the file regardless of configtest saying everything is ok or not."
  # systems restart differently
  if [[ $os == '1' ]]; then
    /etc/init.d/httpd reload
    /etc/init.d/php-fpm reload
    /etc/init.d/mysqld reload
  elif [ $os == '2' ]; then
    service apache2 reload
    service php5-fpm reload
    service php7.0-fpm reload
    service php7.2-fpm reload
  else
    service httpd reload
    service mysqld reload
    service php-fpm reload
  fi
  # attempt to author the https domain if they picked it, let's hope everyone does
  if [[ $protocol == 'https' ]]; then
    cd $HOME
    
    # automatically create domains
    
  else
    elmslnwarn "You really should use https and invest in certs... seriously do it!"
  fi
    # account for ubuntu being a little different here when it comes to apache
  if [ $os == '2' ]; then
    ln -s ${domains}*.conf /etc/apache2/sites-enabled/
  fi
fi

if [[ -n "$zzz_performance" ]]; then
  # Cent 7.x comes with apache 2.4 by default
  if [ $os == '3' ]; then
    cp /var/www/elmsln/scripts/server/apache-2.4/zzz_performance.conf $zzz_performance
  else
    if [ ! -d /etc/apache2/conf-available ]; then
      mkdir /etc/apache2/conf-available
    fi
    cp /var/www/elmsln/scripts/server/zzz_performance.conf $zzz_performance
  fi
  # account for ubuntu being a little different here when it comes to apache
  if [ $os == '2' ]; then
    ln -s $zzz_performance /etc/apache2/sites-enabled/zzz_performance.conf
  fi
fi


# setup infrastructure tools
ln -s /var/www/elmsln/scripts/drush-create-site /usr/local/bin/drush-create-site
ln -s /var/www/elmsln/scripts/drush-command-job /usr/local/bin/drush-command-job
chmod 740 /usr/local/bin/drush-create-site/rm-site.sh

# shortcuts for ease of use
cd $HOME
touch .bashrc
ln -s /var/www/elmsln $HOME/elmsln
echo "alias g='git'" >> .bashrc
echo "alias d='drush'" >> .bashrc
echo "alias l='ls -laHF'" >> .bashrc
echo "alias drs='/usr/local/bin/drush-create-site/rm-site.sh'" >> .bashrc
echo "alias leafy='bash /var/www/elmsln/scripts/elmsln.sh'" >> .bashrc

# setup drush
curl -sS https://getcomposer.org/installer | php
mv composer.phar /usr/local/bin/composer
sed -i '1i export PATH="$HOME/.composer/vendor/bin:$PATH"' .bashrc
sed -i '1i export PATH="$HOME/.config/composer/vendor/bin:$PATH"' .bashrc
source $HOME/.bashrc

# full path to execute in case root needs to log out before it picks it up
php /usr/local/bin/composer global require consolidation/cgr
cgr drush/drush:8.x-dev --prefer-source

# copy in the elmsln server stuff as the baseline for .drush
if [ ! -d $HOME/.drush ]; then
  mkdir $HOME/.drush
fi
yes | cp -rf /var/www/elmsln/scripts/drush/server/* $HOME/.drush/

# stupid ubuntu drush thing to work with sudo
if [[ $os == '2' ]]; then
  ln -s /root/.composer/vendor/drush/drush /usr/share/drush
  ln -s /root/.config/composer/vendor/drush/drush /usr/share/drush
fi
drush cc drush
# setup the standard user accounts to work on the backend
# travis can't do these kind of modifications so drop running them
if [[ $HOME != '/home/travis' ]]; then
  bash /var/www/elmsln/scripts/install/root/elmsln-create-accounts.sh
  # apply ssh settings for innovate contributions
  bash /var/www/elmsln/scripts/utilities/ssh/apply-ssh.sh
  # ensure that the ulmus user doesn't need a tty in order to run
  # this ensures that #491 works correctly
  echo '# ELMSLN users dont need tty' >> /etc/sudoers
  echo 'Defaults:ulmus    !requiretty' >> /etc/sudoers
fi

# install google API so we can tap into it as needed
cd /var/www/elmsln/core/dslmcode/shared/drupal-7.x/libraries/google-api-php-client/
php /usr/local/bin/composer install

# systems restart differently
if [[ $os == '1' ]]; then
  /etc/init.d/httpd reload
  /etc/init.d/mysqld reload
  /etc/init.d/php-fpm reload
elif [ $os == '2' ]; then
  service apache2 reload
  service mysqld reload
  service php5-fpm reload
  service php7.0-fpm reload
  service php7.2-fpm reload
else
  service httpd reload
  service mysqld reload
  service php-fpm reload
fi

# source one last time before hooking crontab up
source $HOME/.bashrc
ulmuscrontab=ulmus
ulmusdrushcrontab=ulmusdrush

if [[ -d "$crontab" ]]; then
  touch $crontab$ulmuscrontab
  touch $crontab$ulmusdrushcrontab
  cat /var/www/elmsln/scripts/server/ulmus_crontab.txt >> $crontab$ulmuscrontab
  cat /var/www/elmsln/scripts/server/ulmusdrush_crontab.txt >> $crontab$ulmusdrushcrontab
  chmod 600 $crontab$ulmuscrontab
  chmod 600 $crontab$ulmusdrushcrontab
  chown ulmus:crontab $crontab$ulmuscrontab
  chown ulmusdrush:crontab $crontab$ulmusdrushcrontab
fi

elmslnecho "Everything should be in place, now you can log out and run the following commands:"
elmslnecho "sudo chown YOURUSERNAME:${webgroup} -R /var/www/elmsln"
elmslnecho "bash /var/www/elmsln/scripts/install/elmsln-install.sh"
elmslnecho "If you go to your IP address you'll be able to watch the build from a cleaner interface"
