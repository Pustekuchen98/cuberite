
// ChunkSender.cpp

// Interfaces to the cChunkSender class representing the thread that waits for chunks becoming ready (loaded / generated) and sends them to clients





#include "Globals.h"
#include "ChunkSender.h"
#include "cWorld.h"
#include "packets/cPacket_MapChunk.h"
#include "packets/cPacket_PreChunk.h"
#include "cBlockEntity.h"





cChunkSender::cChunkSender(void) :
	super("ChunkSender"),
	m_World(NULL)
{
}





cChunkSender::~cChunkSender()
{
	mShouldTerminate = true;
	m_Event.Set();
}





bool cChunkSender::Start(cWorld * a_World)
{
	m_World = a_World;
	return super::Start();
}





void cChunkSender::ChunkReady(int a_ChunkX, int a_ChunkY, int a_ChunkZ)
{
	// This is probably never gonna be called twice for the same chunk, and if it is, we don't mind, so we don't check
	{
		cCSLock Lock(m_CS);
		m_ChunksReady.push_back(cChunkCoords(a_ChunkX, a_ChunkY, a_ChunkZ));
	}
	m_Event.Set();
}





void cChunkSender::QueueSendChunkTo(int a_ChunkX, int a_ChunkY, int a_ChunkZ, cClientHandle * a_Client)
{
	ASSERT(a_Client != NULL);
	{
		cCSLock Lock(m_CS);
		m_SendChunks.push_back(sSendChunk(a_ChunkX, a_ChunkY, a_ChunkZ, a_Client));
	}
	m_Event.Set();
}





void cChunkSender::RemoveClient(cClientHandle * a_Client)
{
	cCSLock Lock(m_CS);
	for (sSendChunkList::iterator itr = m_SendChunks.begin(); itr != m_SendChunks.end();)
	{
		if (itr->m_Client == a_Client)
		{
			itr = m_SendChunks.erase(itr);
			continue;
		}
		++itr;
	}  // for itr - m_SendChunks[]
}





void cChunkSender::Execute(void)
{
	while (!mShouldTerminate)
	{
		cCSLock Lock(m_CS);
		while (m_ChunksReady.empty() && m_SendChunks.empty())
		{
			cCSUnlock Unlock(Lock);
			m_Event.Wait();
			if (mShouldTerminate)
			{
				return;
			}
		}  // while (empty)
		
		if (!m_ChunksReady.empty())
		{
			// Take one from the queue:
			cChunkCoords Coords(m_ChunksReady.front());
			m_ChunksReady.pop_front();
			Lock.Unlock();
			
			SendChunk(Coords.m_ChunkX, Coords.m_ChunkY, Coords.m_ChunkZ, NULL);
		}
		else
		{
			// Take one from the queue:
			sSendChunk Chunk(m_SendChunks.front());
			m_SendChunks.pop_front();
			Lock.Unlock();
			
			SendChunk(Chunk.m_ChunkX, Chunk.m_ChunkY, Chunk.m_ChunkZ, Chunk.m_Client);
		}
	}  // while (!mShouldTerminate)
}





void cChunkSender::SendChunk(int a_ChunkX, int a_ChunkY, int a_ChunkZ, cClientHandle * a_Client)
{
	ASSERT(m_World != NULL);
	
	// Prepare MapChunk packets:
	m_World->GetChunkData(a_ChunkX, a_ChunkY, a_ChunkZ, this);
	cPacket_PreChunk PreChunk(a_ChunkX, a_ChunkZ, true);
	cPacket_MapChunk MapChunk(a_ChunkX, a_ChunkY, a_ChunkZ, m_BlockData);
	
	// Send:
	if (a_Client == NULL)
	{
		m_World->BroadcastToChunk(a_ChunkX, a_ChunkY, a_ChunkZ, PreChunk);
		m_World->BroadcastToChunk(a_ChunkX, a_ChunkY, a_ChunkZ, MapChunk);
	}
	else
	{
		a_Client->Send(PreChunk);
		a_Client->Send(MapChunk);
	}
	
	// Send entity creation packets:
	for (PacketList::iterator itr = m_Packets.begin(); itr != m_Packets.end(); ++itr)
	{
		if (a_Client == NULL)
		{
			m_World->BroadcastToChunk(a_ChunkX, a_ChunkY, a_ChunkZ, **itr);
		}
		else
		{
			a_Client->Send(**itr);
		}
		delete *itr;
	}  // for itr - m_Packets[]
	m_Packets.clear();
}





void cChunkSender::BlockData(const char * a_Data)
{
	memcpy(m_BlockData, a_Data, cChunk::c_BlockDataSize);
}





void cChunkSender::BlockEntity(cBlockEntity * a_Entity)
{
	cPacket * Packet = a_Entity->GetPacket();
	if (Packet != NULL)
	{
		m_Packets.push_back(Packet);
	}
}




void cChunkSender::Entity(cEntity * a_Entity)
{
	// Nothing needed yet, perhaps in the future when we save entities into chunks we'd like to send them upon load, too ;)
}




