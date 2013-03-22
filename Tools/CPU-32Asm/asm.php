<?php

$INFILE = $argv[1];
$OUTFILE = $argv[2];

$gCode = array();
$gCodepos = 0;
$gLabels = array();
$gLastGlobalLabel = "_root_";
$gRelocations = array();

class ParserState
{
	var $ofs;
	var $token;
	var $tokenstr;
}

class Parser
{
	const TOK_NULL = 0;
	const TOK_EOF  = 1;
	const TOK_EOL  = 2;

	protected $_file;
	protected $_line;
	protected $_data;
	
	protected $_cur;
	protected $_last = null;

	protected function _GetC()
	{
		return $this->_data[$this->_cur->ofs];
	}
 
	function __construct()
	{
		$this->_cur = new ParserState();
	}

	function PutBack()
	{
		$this->_cur = $this->_last;
	}

	function GetToken()
	{
		$_last = $_cur;
		
		while( isspace($this->_GetC()) )
			;
		
		$start = $this->_cur->ofs;
		switch($this->_GetC())
		{
		case ',':
			break;
		}
	}
}

function SyntaxError($errstr)
{
	global $gFile, $gLine;
	echo "$gFile:$gLine: Syntax Error: $errstr\n";
}

function EncodingError($errstr)
{
	global $gFile, $gLine;
	echo "$gFile:$gLine: Encoding Error: $errstr\n";
}

function AppendCode( $word )
{
	global $gCode;
	global $gCodepos;
	$gCode[$gCodepos++] = $word;
	//printf("0x%08x\n", $word);
}

function AppendLabel($label)
{
	global $gLabels, $gLastGlobalLabel;
	global $gCodepos;
	if($label[0] == '.')
		$label = $gLastGlobalLabel . $label;
	else
		$gLastGlobalLabel = $label;
	$gLabels[$label] = $gCodepos;
}

function AppendReloc($name, $type, $negate=false)
{
	global $gRelocations, $gLastGlobalLabel;
	global $gCodepos;
	echo "$gCodepos: $type '$name'\n";
	if( $name[0] == '.' )
		$name = $gLastGlobalLabel . $name;
	$gRelocations[] = array( $gCodepos, $type, $name, $negate );
	return 0;
}

function AppendRel24($name)
{
	if( $name == '$' )
		return 0xFFFFFF;
	else
		return AppendReloc($name, "r24-4");
}

function ValueFitsInBits($val, $bits, $isSigned)
{
	if( $isSigned )
	{
		if( $val < -(1<<($bits-1)) )
			return false;
		if( $val >= (1<<($bits-1)) )
			return false;
		return true;
	}
	else
	{
		if($val < 0)
			return false;
		if($val >= (1<<$bits))
			return false;
		return true;
	}
}

function GetReg($name, &$rem=null)
{
	$name = trim($name);
	if( preg_match('~^\s*[Rr]([0-9]|1[0-5])\b\s*(.*)$~', $name, $m) )
	{
		$rem = $m[2];
		return intVal($m[1]);
	}
	if( preg_match('~^\s*PC\b\s*(.*)$~', $name, $m) )
	{
		$rem = $m[1];
		return 16;
	}
	return false;
}

function ReadSymbol($str, &$outstr)
{
	$rv = preg_match('~^\s*([a-zA-Z_][a-zA-Z0-9_]*)(.*)$~', $str, $m);
	if(!$rv)	return false;
	
	$outstr = $m[2];
	return $m[1];
}

function EvaluateExpression($expr, &$outstr, $encType, &$reg1=null, &$reg2=null, &$shift=null)
{
	$reg1 = false;
	$reg2 = false;
	$rv = 0;
	$opr_is_sub = false;
	
	$expr = trim($expr);
	
	if( $expr == "" )
		return 0;

	while( $expr != "" )
	{
		if( ($reg = GetReg($expr, $expr)) !== false )
		{
			if( $opr_is_sub )
				EncodingError("Registers cannot be subtracted");
			if( $expr[0] == '*' || ($expr[0] == '<' && $expr[1] == '<') )
			{
				if( $expr[0] == '*' )
				{
					if( !preg_match('~\*\s*(\d+)\s*(.*)$~', $expr, $m) )
						SyntaxError("Constant register multiplication only");
					$expr = $m[2];
					$mul = intVal($m[1]);
					$mul_log2 = (int)log2($mul);
					if( pow(2, $mul_log2) != $mul )
						EncodingError("Scale is not a power of two");
				}
				else
				{
					if( !preg_match('~<<\s*(\d+)\s*(.*)$~', $expr, $m) )
						SyntaxError("Constant register shift only");
					$expr = $m[2];
					$mul_log2 = intval($m[1]);
				}
				
				if($reg2 !== false)
				{
					EncodingError("Only one scaled register is allowed");
				}
				$reg2 = $reg;
				$shift = $mul_log2;
			}
			else
			{
				// $shift = 0
				if( $reg1 === false )
					$reg1 = $reg;
				else if( $reg2 === false ) {
					$reg2 = $reg;
					$shift = 0;
				}
				else {
					EncodingError("Three registers used in expression");
				}
			}
		}
		else
		{
			$sym = ReadSymbol($expr, &$expr);
			$val = AppendReloc($sym, $encType, $opr_is_sub);
			$rv += $val;
		}
		
		if( $expr == "")
			break;

		if( !preg_match('~([\+\-])\s*(.*)$~', $expr, $m) )
			break;
		$expr = $m[2];
		$opr_is_sub = ($m[1] == '-');
	}
	$outstr = $expr;
	return $rv;
}

function AppendMMM($str, $validmask = 0xFF)
{
	$rv = 0;
	if( preg_match('~^[Rr]([0-9]|1[0-5])$~', $str, $m) )
	{
		$rv = intVal($m[1]) << 16;
	}
	elseif( $str[0] == '[' )
	{
		$str = substr($str, 1);
		$ofs = EvaluateExpression($str, $str, "ad", $reg1, $reg2, $shift);
		if( $ofs == false ) {
			// Oops
		}
		if( $str[0] != ']' ) {
			SyntaxError("Expected ']' at end of expression '$str'");
		}

		if( $reg1 === false )
		{
			EncodingError("MMM requires a base register");
		}

		if( $reg2 !== false )
		{
			if( !ValueFitsInBits($ofs, 9, true) )
				EncodingError("Value $ofs does not fit in 8-bits signed");
			if( !ValueFitsInBits($shift, 3, false) )
				EncodingError("Register shift is too large (max 7, have $shift)");
			if( $reg2 == 16 )
				EncodingError("PC cannot be scaled");
			$rv = ($reg2 << 12) | ($shift << 9) | ($ofs & 0x1FF);
			if( $reg1 == 16 )
				$rv |= (0x7<<24);
			else
				$rv |= (0x5<<24) | ($reg1 << 16);
		}
		else
		{
			if( !ValueFitsInBits($ofs, 16, true) )
				EncodingError("Value $ofs does not fit in 16-bits signed");
			$rv = ($ofs & 0xFFFF);
			if( $reg1 == 16 )
				$rv |= (0x6<<24);
			else
				$rv |= (0x4<<24) | ($reg1 << 16);
		}
	}
	else
	{
		// Constant
		// TODO: Validate
		$const = intval($str, 0);
		
		// imm20?
		if( ValueFitsInBits($const, 20, false) )
			$rv = (0x01<<24) | $const;
		// imm16s
		else if( ValueFitsInBits($const, 16, true) )
			$rv = (0x03<<24) | $const;
		// imm16
		else
		{
			$val = $const;
			for( $shift = 1; $shift < 32 && !($val & 1); $shift ++ )
			{
				$val >>= 1;
				if( $val < (1<<15) ) {
					$rv = (0x02<<24) | ($shift << 16) | ($val);
					break;
				}
			}
			
			// TODO: imm16s again
		}
	
		if( $rv == 0 )
		{
			EncodingError("Value $str cannot be encoded");
		}
	}
	
	$mode = ($rv >> 24) & 7;
	if( !($validmask & (1 << $mode)) )
		return false;
	
//	printf("0x%08x\n",$rv);
	return $rv;
}

function ParseAluArgs($op, $args)
{
	$ret = ($op&7) << 27;
	if($args[0] == '[') {
		preg_match('~(\[[^\]]+\])\s*,\s*(r\d+)~', $args, $m);
		//parse mmm
		$mstr = $m[1];
		$rstr = $m[2];
		$ret |= (1<<30);
	}
	else {
		$pos = strpos($args, ',');	assert($pos !== false);
		$rstr = trim(substr($args, 0, $pos));
		$mstr = trim(substr($args, $pos+1));
	}
	$reg = GetReg($rstr);
	$mmm = AppendMMM($mstr);
	
	$ret |= $reg << 20;
	$ret |= $mmm;
	return $ret;
}

function ParseCall($cname, $isCall, $args)
{
	if( preg_match('~^rel\s(.*)~', $args, $m) ) {
		$ret = ($isCall ? 0x9 : 0x8) << 28;
		$ret |= AppendRel24($m[1]);
		$cofs = 24;
	}
	else {
		$ret = ($isCall ? 0xA8 : 0xA0) << 24;
		$ret |= AppendMMM($args);
		$cofs = 20;
	}
	
	$cc = 16;
	switch($cname)
	{
	case 'z':	$cc = 0x0;	break;	// ZF=1
	case 'nz':	$cc = 0x0;	break;	// ZF=0
	case 'al':	$cc = 0xE;	break;	// always
	case 'nv':	$cc = 0xF;	break;	// never
	case '':	$cc = 0xE;	break;	// Default to always
	default:
		SyntaxError("Unknown condition code '$cname'");
		return false;
	}
	
	$ret |= ($cc << $cofs);
	return $ret;
}

function ParseShift($isShift, $isRight, $useCarry, $args)
{
	$varg = explode(",", $args, 3);
	if( count($varg) == 3 )
		list($dst,$reg,$amt) = $varg;
	else {
		$dst = $reg = $varg[0];
		$amt = $varg[1];
	}

	$dst = trim($dst);
	$reg = trim($reg);
	$amt = trim($amt);

	$dstr = GetReg($dst);
	$srcr = GetReg($reg);
	if( ($shiftr = GetReg($amt)) !== false )
	{
		$ret = ($dstr << 12) | (1 << 4) | ($srcr << 8) | $shiftr;
	}
	else
	{
		$amtv = intval($amt, 0);
		if( $amtv < 1 || $amtv >= 32 ) {
			SyntaxError("Shift value out of range (1-31 incl) $amtv");
			return false;
		}
		$ret = ($dstr << 12) | (0 << 4) | ($srcr << 8) | $amtv;
	}
	$ret |= ($useCarry ? 1 << 5 : 0);
	$ret |= ($isRight  ? 1 << 6 : 0);
	$ret |= ($isShift  ? 1 << 7 : 0);
	$ret |= (0xB802 << 16);
	return $ret;
}

function ParseNWLS($maxOfs, $args)
{
	$rv = preg_match('~^(r\d+)(?:\.(\d))?\s*,\s*\[\s*([^\]]*)\]$~', $args, $m);
	if(!$rv)
		SyntaxError("NWLS arguments are malformed ('$args')");
	$lreg = GetReg($m[1]);
	$lofs = intval($m[2]);
	$mem = $m[3];
	$ofs_bits = EvaluateExpression($mem, $mem, "a16-1", $rreg, $ireg_sbf);
	if( $mem != "" )
		SyntaxError("Junk at and of expression '$mem'");
	if( $ireg_sbf !== false )
		SyntaxError("NWLS does not support scaled registers");
	
	if( $lofs > $maxOfs )
		SyntaxError("Register offset too large ($rofs > $maxOfs)");
	
	if( !ValueFitsInBits($ofs_bits, 16, true) )
		EncodingError("Offset is too large to encode ($ofs_bits not 16-bit)");
	
	return ($lofs<<25) | ($lreg<<20) | ($rreg<<16) | ($ofs_bits&0xFFFF);
}

function AssembleFile($filename)
{
	global $gFile, $gLine;
	
	$gFile = $filename;
	$gLine = 0;

	$fp = fopen($filename, "r");

	while($line = fgets($fp))
	{
		if( strpos($line, ";") !== false )
			$line = substr($line, 0, strpos($line, ";"));

		$line = trim($line);
		if( $line == "" )
			continue ;

		if( preg_match('~^\[(.*)\]$~', $line, $m) )
		{
			// TODO: ORG etc
			continue;
		}

		if( strpos($line, ":") !== false )
		{
			$label = substr($line, 0, strpos($line, ":"));
			$line = substr($line, strpos($line, ":")+1);

			AppendLabel($label);		
		}

		$line = trim($line);
		if( $line == "" )
			continue ;

		if( preg_match('~^([a-zA-Z]+)(\.[a-z][a-z]?)?(?:\s+(.*))?$~', $line, $m) )
		{
			$code = trim($m[1]);
			$cc = trim($m[2], '.');
			$args = trim($m[3]);

			if( $code == "" )
				continue ;

			//echo $code, " -- ", $args, "\n";
			
			switch( $code )
			{
			case "jmp":
				AppendCode( ParseCall($cc, false, $args) );
				$cc = "";
				break;
			case "call":
				AppendCode( ParseCall($cc, true, $args) );
				$cc = "";
				break;
			case "push":
				AppendCode( (0xB00 << 20) | AppendMMM($args));
				break;
			case "pop":
				AppendCode( (0xB01 << 20) | AppendMMM($args, 0xF1));
				break;

			case "lea":
				list($regstr,$mstr) = explode(",", $args);
				$regstr = trim($regstr);
				$mstr = trim($mstr);
				$reg = GetReg($regstr);
				AppendCode( (0xBC << 24) | ($reg << 20) | AppendMMM($mstr, 0xF0) );
				break;		

			case "mov":	AppendCode( ParseAluArgs(0, $args) );	break;
			case "and":	AppendCode( ParseAluArgs(1, $args) );	break;
			case "or":	AppendCode( ParseAluArgs(2, $args) );	break;
			case "xor":	AppendCode( ParseAluArgs(3, $args) );	break;
			case "add":	AppendCode( ParseAluArgs(4, $args) );	break;
			case "sub":	AppendCode( ParseAluArgs(5, $args) );	break;
			case "cmp":	AppendCode( ParseAluArgs(6, $args) );	break;
			case "test":	AppendCode( ParseAluArgs(7, $args) );	break;

			case "shr":	AppendCode( ParseShift(true, true, false, $args) );	break;
			case "scr":	AppendCode( ParseShift(true, true, true, $args) );	break;
			case "shl":	AppendCode( ParseShift(true, false, false, $args) );	break;
			case "scl":	AppendCode( ParseShift(true, false, true, $args) );	break;
			
			case "ror":	AppendCode( ParseShift(false, true, false, $args) );	break;
			case "rcr":	AppendCode( ParseShift(false, true, true, $args) );	break;
			case "rol":	AppendCode( ParseShift(false, false, false, $args) );	break;
			case "rcl":	AppendCode( ParseShift(false, false, true, $args) );	break;

			case "db":
			case "dh":
			case "dw":
				echo "TODO: Inline data\n";
				break;

			case "strb":	AppendCode( (0xC8 << 24) | ParseNWLS(3, $args) );	break;
			case "ldrb":	AppendCode( (0xC9 << 24) | ParseNWLS(3, $args) );	break;
			case "strw":	AppendCode( (0xC0 << 24) | ParseNWLS(1, $args) );	break;
			case "ldrw":	AppendCode( (0xC1 << 24) | ParseNWLS(1, $args) );	break;
			case "ldrws":	AppendCode( (0xC4 << 24) | ParseNWLS(0, $args) );	break;
			case "ldrwz":	AppendCode( (0xC5 << 24) | ParseNWLS(0, $args) );	break;
			case "ldrbs":	AppendCode( (0xC6 << 24) | ParseNWLS(0, $args) );	break;
			case "ldrbz":	AppendCode( (0xC7 << 24) | ParseNWLS(0, $args) );	break;

			default:
				echo "Unknown operation '$code'\n";
				break;
			}
			if( $cc != "" )
				SyntaxError("Use of condition code on non-supporting operation");

			continue ;
		}
		echo "UNK - ", $line, "\n";
	}
	fclose($fp);
}

AssembleFile($INFILE);

foreach($gRelocations as $reloc)
{
	$pos = $reloc[0];
	$type = $reloc[1];
	$sym = $reloc[2];
	$negate = $reloc[3];
	
	if( !isset($gLabels[$sym]) )
		die("Unknown symbol '$sym'");
	$symval = $gLabels[ $sym ] * 4;
	
	switch($type)
	{
	case 'r24-4':
		$val = $symval/4 - $pos;
		$bits = 24;
		$bSigned = true;
		break;
	case 'a16-1':
		$val = $symval;
		$bits = 16;
		$bSigned = true;
		break;
	case 'ad':
		$op = $gCode[$pos];
		switch( ($op>>24) & 0x85 )
		{
		case 0x84:	// a16-1
			$val = $symval;
			$bits = 16;
			$bSigned = true;
			break;
		case 0x85:	// a8-1
			$val = $symval;
			$bits = 9;
			$bSigned = true;
			break;
		default:
			die("Relocation auto-detect failed (".dechex($op>>24).")");
		}
		break;
	default:
		die("Unknown relocation type '$type'");
	}
	
	if( !ValueFitsInBits($val, $bits, $bSigned) )
		die("Relocation too big to fit ($type,$bits,$bSigned)");
	$vmask = (1<<$bits)-1;
	if( $negate )	$val = -$val;
	$ov = $gCode[$pos] & $vmask;
	$gCode[$pos] &= ~$vmask;
	$gCode[$pos] |= ($ov + $val) & $vmask;
}

$fp = fopen($OUTFILE, "wb");
foreach($gCode as $dword)
{
	printf("%08x ", $dword);
	fwrite($fp, pack("V", $dword));
}
echo "\n";
fclose($fp);

?>
