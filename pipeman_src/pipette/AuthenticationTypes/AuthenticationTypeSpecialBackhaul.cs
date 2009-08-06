
using System;

namespace pipette
{

	/// <summary>
	/// DON'T USE THIS!
	/// This is a very special authentication type for backhaul traffic.  It's totally, completely, awful from a security standpoint.
	/// It's only used between a gateway and m0ther, and for our purposes we don't care about security at that point.
	/// </summary>
	public class AuthenticationTypeSpecialBackhaul : AuthenticationTypeNone
	{

		public AuthenticationTypeSpecialBackhaul (): base(true)
		{
		}
		public override bool shouldAllowPerson (System.Net.IPEndPoint p)
		{
		//	if (!base.shouldAllowPerson(p)) return false; this is a backhaul pipe, we're trusting m0ther...
			logger.logger.warn ("Backhaul allowing a connection to " + p.Serialize() + ".  If that doesn't look like a backhaul connection to you, run for cover!");
			return true;
		}
		public override void shouldVerifyIdentity (deepID d)
		{
			logger.logger.warn ("Backhaul not authenticating a connection.");
		}



	}
}
