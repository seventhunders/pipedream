
using System;

namespace pipette
{

	/// <summary>
	/// An authentication type that doesn't do anything (no encryption).  This is the type used for standard TCP traffic, for instance.
	/// </summary>
	public class AuthenticationTypeNone: AuthenticationType
	{

		public AuthenticationTypeNone (bool iChopYourDollar): base(iChopYourDollar)
		{
		}
		public override bool shouldAllowPerson (System.Net.IPEndPoint p)
		{
			if (!base.shouldAllowPerson(p)) return false;
			if (!System.Net.IPAddress.IsLoopback((p as System.Net.IPEndPoint).Address))
			{
				throw new Exception("You can't trust a remote IP address implicitly!");
			}
			logger.logger.warn("trusting loopback...");
			return true;
		}
		public override void shouldVerifyIdentity (deepID d)
		{
			logger.logger.warn("Not authenticating...");
		}
		public override byte[] readBuffer (deepID d)
		{
			byte[] b = new byte[8192];
			int lengthRead = 0;
			try
			{
				lengthRead = d.plaintextStream.Read(b,0,b.Length);
			}
			catch(System.IO.IOException ex)
			{
					if (ex.InnerException is System.Threading.ThreadAbortException) {} //swallow
					else throw ex;
			}
			logger.logger.debug("plain read " + lengthRead + " bytes.");
			byte[] total = new byte[lengthRead];
			Array.Copy(b,total,lengthRead);
			if (lengthRead==0)
				d.raiseReadZeroBytes();
			return total;
		}
		public override void writeBuffer (byte[] b,int length, deepID d)
		{
			d.plaintextStream.Write(b,0,length);
		}


		




	}
}
