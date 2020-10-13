<!DOCTYPE html>
<?php
$cfgdir = 'HHHH';

$cfg = array();

function ParseKVFile(string $filename, &$kvarray)
{
	if ($lines = file($filename, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES)) {
		foreach ($lines as $line) {
			$line = trim($line);
			if ($line[0] == '#') continue;
			if (! strpos($line, '=')) continue;
			list( $key, $value ) = explode('=', $line);
			if ("'" == $value[0])
				list ( $value ) = explode("'", substr($value, 1));
			else
				list ( $value ) = explode(' ', $value);
			$value = trim($value);
			$kvarray[$key] = $value;
		}
	}
}

function GetIP(string $type)
{
	if ('internal' == $type) {
		$iplist = explode(' ', `hostname -I`);
		foreach ($iplist as $ip) {
			if (strpos($ip, '.')) break;
		}
	} else if ('ipv6' == $type)
		$ip = trim(`curl --silent -6 icanhazip.com`);
	else if ('ipv4' == $type)
		$ip = trim(`curl --silent -4 icanhazip.com`);
	else
		$ip = '';
	return $ip;
}

function SecToString(int $sec) {
	if ($sec >= 86400)
		return sprintf("%0.2f days", $sec/86400);
	$hrs = intdiv($sec, 3600);
	$sec %= 3600;
	$min = intdiv($sec, 60);
	$sec %= 60;
	if ($hrs) return sprintf("%2d hr  %2d min", $hrs, $min);
	if ($min) return sprintf("%2d min %2d sec", $min, $sec);
	return sprintf("%2d sec", $sec);
}

function MyAndSfxToQrz(string $my, string $sfx)
{
	$my = trim($my);
	$sfx = trim($sfx);
	if (0 == strlen($my)) {
		$my = 'Empty MYCall ';
	} else {
		if (strpos($my, ' '))
			$link = strstr($my, ' ', true);
		else
			$link = $my;
		if (strlen($sfx))
			$my .= '/'.$sfx;
		$len = strlen($my);
		$my = '<a*target="_blank"*href="https://www.qrz.com/db/'.$link.'">'.$my.'</a>';
		while ($len < 13) {
			$my .= ' ';
			$len += 1;
		}
	}
	return $my;
}

ParseKVFile($cfgdir . '/mvoice.cfg', $cfg);
?>

<html>
<head>
<title>DigitalVoice Dashboard</title>
<meta http-equiv="refresh" content="10">
</head>
<body>
<h2>DigitalVoice <?php echo $cfg['StationCall'];?> Dashboard</h2>

<?php
$showorder = 'MO,LH,SY';
$showlist = explode(',', $showorder);
foreach($showlist as $section) {
	switch ($section) {
		case 'PS':
			if (`ps -aux | grep -e qn -e MMDVMHost | wc -l` > 2) {
				echo 'Processes:<br><code>', "\n";
				echo str_replace(' ', '&nbsp;', 'USER       PID %CPU %MEM    VSZ   RSS TTY      STAT START   TIME COMMAND<br>'), "\n";
				$lines = explode("\n", `ps -aux | grep -e qngateway -e qnlink -e qndtmf -e qndvap -e qnitap -e qnrelay -e qndvrptr -e qnmodem -e MMDVMHost | grep -v grep`);
				foreach ($lines as $line) {
					echo str_replace(' ', '&nbsp;', $line), "<br>\n";
				}
				echo '</code>', "\n";
			}
			break;
		case 'SY':
			echo 'System Info:<br>', "\n";
			$hn = trim(`uname -n`);
			$kn = trim(`uname -rmo`);
			$osinfo = file('/etc/os-release', FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
			foreach ($osinfo as $line) {
				list( $key, $value ) = explode('=', $line);
				if ($key == 'PRETTY_NAME') {
					$os = trim($value, '"');
					break;
				}
			}
			$cu = trim(`cat /proc/cpuinfo | grep Model`);
			if (0 == strlen($cu))
				$cu = trim(`cat /proc/cpuinfo | grep "model name"`);
			$culist = explode("\n", $cu);
			$mnlist = explode(':', $culist[0]);
			$cu = trim($mnlist[1]);
			if (count($culist) > 1)
				$cu .= ' ' . count($culist) . ' Threads';
			if (file_exists('/opt/vc/bin/vcgencmd'))
				$cu .= ' ' . str_replace("'", '&deg;', trim(`/opt/vc/bin/vcgencmd measure_temp`));
			echo '<table cellpadding="1" border="1" style="font-family: monospace">', "\n";
			echo '<tr><td style="text-align:center">CPU</td><td style="text-align:center">Kernel</td><td style="text-align:center">OS</td><td style="text-align:center">Hostname</td></tr>', "\n";
			echo '<tr><td style="text-align:center">', $cu, '</td><td style="text-align:center">', $kn, '</td><td style="text-align:center">', $os, '</td><td style="text-align:center">', $hn, '</td></tr></table><br>', "\n";
			break;
		case 'LH':
			echo 'Last Heard:<br><code>', "\n";
			$rstr = 'MyCall/Sfx   Mod Via       Time<br>';
			echo str_replace(' ', '&nbsp;', $rstr), "\n";
			$db = new SQLite3($cfgdir . '/qn.db', SQLITE3_OPEN_READONLY);
			$ss = 'SELECT callsign,sfx,module,reflector,strftime("%s","now")-lasttime FROM LHEARD ORDER BY 5 LIMIT 20 ';
			if ($stmnt = $db->prepare($ss)) {
				if ($result = $stmnt->execute()) {
					while ($row = $result->FetchArray(SQLITE3_NUM)) {
						$rstr = MyAndSfxToQrz($row[0], $row[1]).' '.$row[2].'  '.$row[3].' '.SecToString(intval($row[4])).'<br>';
						echo str_replace('*', ' ', str_replace(' ', '&nbsp;', $rstr)), "\n";
					}
					$result->finalize();
				}
				$stmnt->close();
			}
			$db->Close();
			echo '</code><br>', "\n";
			break;
		case 'IP':
			echo 'IP Addresses:<br>', "\n";
			echo '<table cellpadding="1" border="1" style="font-family: monospace">', "\n";
			echo '<tr><td style="text-align:center">Internal</td><td style="text-align:center">IPV4</td><td style="text-align:center">IPV6</td></tr>', "\n";
			echo '<tr><td>', GetIP('internal'), '</td><td>', GetIP('ipv4'), '</td><td>', GetIP('ipv6'), '</td></tr>', "\n";
			echo '</table><br>', "\n";
			break;
		case 'MO':
			echo 'Modules:<br>', "\n";
			$db = new SQLite3($cfgdir . '/qn.db', SQLITE3_OPEN_READONLY);
			echo "<table cellpadding='1' border='1' style='font-family: monospace'>\n";
			echo '<tr><td style="text-align:center">Module</td><td style="text-align:center">Modem</td><td style="text-align:center">Link</td><td style="text-align:center">Linked Time</td><td style="text-align:center">Link IP</td></tr>', "\n";
			$linkstatus = 'Unlinked';
			$address = '';
			$ctime = '';
			$ss = 'SELECT ip_address,to_callsign,to_mod,strftime("%s","now")-linked_time FROM LINKSTATUS WHERE from_mod=' . "'" . $cfg['Module'] . "';";
			if ($stmnt = $db->prepare($ss)) {
				if ($result = $stmnt->execute()) {
					if ($row = $result->FetchArray(SQLITE3_NUM)) {
						$linkstatus = str_pad(trim($row[1]), 7).$row[2];
						$address = $row[0];
						$ctime = SecToString(intval($row[3]));
					}
					$result->finalize();
				}
				$stmnt->close();
			}
			echo '<tr><td style="text-align:center">', $cfg['Module'], '</td><td style="text-align:center">DigitalVoice</td><td style="text-align:center">', $linkstatus, '</td><td style="text-align:right">', $ctime, '</td><td style="text-align:center">', $address, '</td></tr>', "\n";
			echo '</table><br>', "\n";
			$db->close();
			break;
		default:
			echo 'Section "', $section, '" was not found!<br>', "\n";
			break;
	}
}
?>
<br>
<p align="right">DigitalVoice Dashboard Version 1.0 Copyright &copy; by Thomas A. Early, N7TAE.</p>
</body>
</html>
