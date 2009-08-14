
using System;

namespace pipette
{

/// <summary>
/// A deepID represents a (bidirectional) stream between two entities (you and a remote host).
	/// Nevermind about whether it uses TCP, UDP, or carrier pidgeon.
/// </summary>
	public class deepID
	{
		/// <summary>
		/// This is used by the PipeEndType to do get a handle on the underlying protocol.
		/// If you're implementing your own PipeEndType, consider storing a clever handle here.
		/// If you're not, leave this alone.
		/// </summary>
		public object internal_use;
		
		public delegate void readZeroBytesHandler(deepID d);
		public event readZeroBytesHandler readZeroBytes;
		public void raiseReadZeroBytes()
		{
			readZeroBytes(this);
		}
		public System.Threading.Thread read_thread;
		public deepID related;
		/// <summary>
		/// This is a plain text stream for the underlying protocol.  The authenticationTypes
		/// make use of this, and it's also used for any other unencrypted traffic or before
		/// encryption is set up, as well as by the encryption itself
		/// </summary>
		public System.IO.Stream plaintextStream;
		/// <summary>
		/// An AESKey for this deepID, if it exists.  This should be set up during the
		/// AuthenticationType's verifyIdentity call
		/// </summary>
		public byte[] aeskey;
		
		public void unregisterallCakeIsALie_remoteEndHungupHandlers()
		{
			logger.logger.debug("Unregistering remotendhungup");
			cakeisalie_remoteEndHungup = null;
		}
		
		public delegate void cakeisalie_remoteEndHungupHandler(deepID who);
		public event cakeisalie_remoteEndHungupHandler cakeisalie_remoteEndHungup;
		public void raiseCakeIsALie_remoteEndHungup()
		{
			this.cakeisalie_remoteEndHungup(this);
		}
		
		
		public void unregisterAllConnectionClosedHandlers()
		{
			logger.logger.debug("unregistering connection closed");
			connectionClosed = null;
		}
		public delegate void  connectionClosedHandler (deepID who);
		public event connectionClosedHandler connectionClosed;
		public deepID ()
		{
		}
		public bool raiseConnectionClosed()
		{
			if (connectionClosed != null)
			{
				this.connectionClosed(this);
				return true;
			}
			else
			{
				logger.logger.warn("Nobody cares that this connection was closed (WTF?)");
				return false;
			}
		}
	}
}
