
using System;

namespace pipette
{
	/// <summary>
	/// Throw this if you decide the connector is untrusted
	/// </summary>
	public class VerificationFailedException: Exception
	{
	}
	/// <summary>
	/// An authenticaton type specifies the authentication and encryption protocol for a pipeendpoint
	/// </summary>
	/// 
	public abstract class AuthenticationType
	{


		private bool iChopYourDollar;
		public AuthenticationType (bool iChopYourDollar)
		{
			this.iChopYourDollar = iChopYourDollar;
		}
		/// <summary>
		/// Is this endpoint allowed to connect?  This is a quick sanity check and not intended to amount to serious security.
		/// However, many pipeEndTypes will hard-shutdown endpoints that don't meet this criteria.
		/// If you're using AuthenticationTypeNone, this may be your only defense against attack.
		/// </summary>
		/// <param name="p">
		/// A <see cref="System.Net.EndPoint"/>
		/// </param>
		/// <returns>
		/// A <see cref="System.Boolean"/>
		/// </returns>
		/// 
		public virtual bool shouldAllowPerson(System.Net.IPEndPoint p)
		{
			if (!p.Address.ToString().StartsWith("10.") && p.Address.ToString()!="127.0.0.1" && !iChopYourDollar && !p.Address.ToString().StartsWith("192.168"))
			{
				logger.logger.warn("Not acceping a connection from " + p.Address.ToString() + " because it doesn't begin with 10.x.  To let this connection through, use --I-CHOP-YOUR-DOLLAR next time.");
				return false;
			}
			return true;
		}
		
		/// <summary>
		/// This is responsible for verifying the other's identity.
		/// You should throw an VerificationFailedException if verification fails.
		/// You should set anything necessary for writeBuffer and readBuffer to function here.
		/// If you're inheriting from AuthenticationTypeAESLike, set the deepID's aeskey
		/// in this function, preferably based on the authentication handshake.
		/// </summary>
		/// <param name="d">
		/// A <see cref="deepID"/>
		/// </param>
		public abstract void shouldVerifyIdentity(deepID d);
		
/// <summary>
/// Performs a write operation.  This function should be secure for most authenticaton types.
/// </summary>
/// <param name="b">
/// A <see cref="System.Byte[]"/>
		/// The buffer to write.
/// </param>
/// <param name="length">
		/// How many bytes from the buffer should be written
/// A <see cref="System.Int32"/>
/// </param>
/// <param name="d">
		/// The deepID who should be written to
/// A <see cref="deepID"/>
/// </param>
		public abstract void writeBuffer(byte[] b, int length, deepID d);
		
		/// <summary>
		/// Perform a read operation.  This function should be secure for most authenticaton types.
		/// </summary>
		/// <param name="d">
		/// A <see cref="deepID"/>
		/// </param>
		/// <returns>
		/// A buffer that is exactly the size of the number of bytes read.  CRITICAL that this assertion holds.
		/// A <see cref="System.Byte[]"/>
		/// </returns>
		public abstract byte[] readBuffer(deepID d);
	}
}
