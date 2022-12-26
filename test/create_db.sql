CREATE DATABASE IF NOT EXISTS `dcbook_hw_test`;
USE dcbook_hw_test;

CREATE TABLE IF NOT EXISTS `tb_flow_info`(
    `fid`       INT UNSIGNED AUTO_INCREMENT,
    `srcip`     INT UNSIGNED,
    `dstip`     INT UNSIGNED,
    `srcport`   INT UNSIGNED,
    `dstport`   INT UNSIGNED,
    `protocol`  INT UNSIGNED,
    `pkt_tot`   INT UNSIGNED DEFAULT 0,
    `pkt_max`   INT UNSIGNED DEFAULT 0,
    `byte_tot`  INT UNSIGNED DEFAULT 0,
    `byte_max`  INT UNSIGNED DEFAULT 0,
    `wid_begin` INT UNSIGNED DEFAULT 0,
    `wid_last`  INT UNSIGNED DEFAULT 0,
    PRIMARY KEY ( `srcip`, `dstip`, `srcport`, `dstport`, `protocol`),
    KEY (`fid`)
)ENGINE=InnoDB DEFAULT CHARSET=utf8;


CREATE TABLE IF NOT EXISTS `tb_flow_counters`(
    `fid`       INT UNSIGNED,
    `offset`    INT UNSIGNED,
    `count`     INT UNSIGNED DEFAULT 0,
    PRIMARY KEY ( `fid`, `offset`)
)ENGINE=InnoDB DEFAULT CHARSET=utf8;


ALTER TABLE `tb_flow_info`
    ADD CONSTRAINT fk_fid
    FOREIGN KEY(`fid`)
    REFERENCES tb_flow_counters(fid);


desc tb_flow_info;