#include "netmessages.h"
#include "bitbuf.h"
#include "const.h"
#include "../engine/net_chan.h"
#include "mathlib/mathlib.h"
#include "networkstringtabledefs.h"
#include "../engine/event_system.h"
#include "../engine/dt.h"
#include "tier0/vprof.h"
#include "convar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define NETMSG_TYPE_BITS	5	// should be 5 bits in gmod
#define NETMSG_LENGTH_BITS	16	// should be 16 bits

static char s_text[1024];

bool SVC_UserMessage::WriteToBuffer( bf_write &buffer )
{
	m_nLength = m_DataOut.GetNumBitsWritten();

	Assert( m_nLength < (1 << NETMSG_LENGTH_BITS) );
	if ( m_nLength >= (1 << NETMSG_LENGTH_BITS) )
		return false;

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteByte( m_nMsgType );
	buffer.WriteUBitLong( m_nLength, NETMSG_LENGTH_BITS );  // max 256 * 8 bits, see MAX_USER_MSG_DATA
	
	return buffer.WriteBits( m_DataOut.GetData(), m_nLength );
}

bool SVC_UserMessage::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_UserMessage::ReadFromBuffer" );
	m_nMsgType = buffer.ReadByte();
	m_nLength = buffer.ReadUBitLong( NETMSG_LENGTH_BITS ); // max 256 * 8 bits, see MAX_USER_MSG_DATA
	m_DataIn = buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_UserMessage::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: type %i, bytes %i", GetName(), m_nMsgType, Bits2Bytes(m_nLength) );
	return s_text;
}