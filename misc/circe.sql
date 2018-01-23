
CREATE DATABASE /*!32312 IF NOT EXISTS*/ `circe` /*!40100 DEFAULT CHARACTER SET utf8 */;

USE `circe`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `PilotCompass` (
  `id` varchar(255) NOT NULL,
  `last_access_time` datetime NOT NULL,
  `version` int(10) unsigned NOT NULL,
  `value` text NOT NULL,
  PRIMARY KEY (`id`),
  KEY `last_access_time` (`last_access_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;
