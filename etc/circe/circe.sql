
CREATE DATABASE /*!32312 IF NOT EXISTS*/ `circe` /*!40100 DEFAULT CHARACTER SET utf8 */;

USE `circe`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `Pilot::Compass` (
  `compass_key` char(40) NOT NULL,
  `last_access_time` datetime NOT NULL,
  `value` text NOT NULL,
  `version` bigint(20) unsigned NOT NULL,
  PRIMARY KEY (`compass_key`),
  KEY `last_access_time` (`last_access_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;
