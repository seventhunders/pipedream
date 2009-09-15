
using System;

namespace pipette
{

/// <summary>
/// Special pipe that acts as a gateway to a remote host.
	/// Uses Backhaul authentication for the remote leg, which is very dangerous if you're not clever.
	/// You probably shouldn't use this pipe type.
/// </summary>
	public class gatewayPipe: Pipe
	{

		public gatewayPipe (string rsa): base(new PipeEndTypeLocalBind(System.Net.IPAddress.Any,12345, new AuthenticationTypeSpecialGateway(rsa)),
		                            new PipeEndTypeTCPClient(magic.DIRECT_MOTHER_HOST,magic.DIRECT_MOTHER_PORT, new AuthenticationTypeSpecialBackhaul()))
		{
			
		}
	}
}
