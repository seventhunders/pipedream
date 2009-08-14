
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
			{
				try
				{
					remotep.authenticationType.writeBuffer(b,b.Length,remote);
				}
				catch (System.IO.IOException ex){
					logger.logger.warn("Something bad happened: " + ex.Message);
					if (ex.InnerException != null)
					{
						logger.logger.warn(ex.InnerException.Message);
					}
					System.Threading.Thread.CurrentThread.Abort();
				}
			}
			else //somebody should investigate why this is 0
			{
				logger.logger.warn("WTF?");
			}
		}
	
		public abstract string description(deepID d);
		public abstract void closeConnection (deepID d);
		
	}
}
