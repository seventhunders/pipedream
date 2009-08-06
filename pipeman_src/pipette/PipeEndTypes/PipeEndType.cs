
using System;

namespace pipette
{

	/// <summary>
	/// Specifies an underlying protocol for a single endpoint, for instance,
	/// binding to a local port or connecting via TCP to a remote host.
	/// Instances do NOT describe individual connections.  For that you need
	/// a deepID, for which this class is a factory.
	/// </summary>
	public abstract class PipeEndType
	{
		protected AuthenticationType authenticationType;
		public PipeEndType (AuthenticationType authenticationType)
		{
			this.authenticationType = authenticationType;
		}
		
			public abstract void start(deepID related);
			public abstract void stop();
		
		
		public delegate void endpointEstablishedHandler(deepID d, deepID related);
		public event endpointEstablishedHandler endpointEstablished;
		public abstract void performAuthentication(deepID d);
		protected void raiseEndpointEstablishedHandler(deepID d,deepID related)
		{
			endpointEstablished(d,related);
		}
		public void copyTo(deepID local, deepID remote,PipeEndType remotep)
		{
			byte[] b = authenticationType.readBuffer(local);
			if (b.Length!=0)
				remotep.authenticationType.writeBuffer(b,b.Length,remote);
			else //somebody should investigate why this is 0
			{
				
			}
		}
		public delegate void connectionClosedHandler(deepID d);
		/// <summary>
		/// WARNING!  After this event fires once, you will be unregistered as a listener.
		/// This is because it keeps the pipe.cs code clean.  If you want to keep receiving 
		/// this event, you must re-register when it fires.
		/// </summary>
		protected event connectionClosedHandler connectionClosed;
		protected void raiseConnectionClosed(deepID d)
		{
			logger.logger.info("Pool's closed (AIDS)");
			logger.logger.info(new System.Diagnostics.StackTrace().ToString());
			connectionClosed(d);
			connectionClosed = null;
		}
		public void addconnectionClosedHandler(connectionClosedHandler c)
		{
			if (this.connectionClosed!=null)
			{
				//If a "TO' closes the connection, execution resumes from the "FROM" anon delegate.
				//In this case, it will try to re-register the "TO" connectionclosedhandler when one already exists.
				//since there's no good way to test for that in pipe.cs, we test for it here.
				//note that we must replace the old anon delegate with the new one because the new one has references to different threads.
				//if pipe.cs calls abort on the old threads, we leave a lot of new ones lying around
				//and they throw fun errors
				logger.logger.debug("Replacing old handler with this new one.");
				this.connectionClosed = c;
			}
			else
			{
				this.connectionClosed += c;

			}
		}
		
		public virtual void closeConnection(deepID d)
		{
			connectionClosed = null;
		}
		
		public abstract string description(deepID d);
		
	}
}
