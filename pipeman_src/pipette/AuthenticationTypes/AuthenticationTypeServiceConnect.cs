
using System;

namespace pipette
{


	public class AuthenticationTypeServiceConnect: AuthenticationTypeAESLike
	{
		string otpkey;
		string otp;
		public AuthenticationTypeServiceConnect (bool iChopYourDollar, string otpkey, string otp): base(iChopYourDollar)
		{
			this.otpkey = otpkey;
			this.otp = otp;
		}
		public override bool shouldAllowPerson (System.Net.IPEndPoint p)
		{
			if (!base.shouldAllowPerson(p)) return false;
			//we've already established in earlier in python that this is the right address for the service
			return true;
		}
		public override void shouldVerifyIdentity (deepID d)
		{
			//send otp key
			byte[] botpkey = System.Text.Encoding.ASCII.GetBytes(otpkey);
			msgBase.write(d.plaintextStream,botpkey);
			logger.logger.debug("I'm using otk: " + otp);
			System.Security.Cryptography.Rfc2898DeriveBytes derive = new System.Security.Cryptography.Rfc2898DeriveBytes(otp,AuthenticationTypeServiceAccept.salt);
			d.aeskey = derive.GetBytes(cryptlib.AES.total_key_size);
		}


		 
		
	}
}
