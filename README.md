# Task Scheduler ALPC Vulnerability (VU#906424)
Credit to SandBoxEscaper and his randomrepo (https://github.com/SandboxEscaper/randomrepo)

To learn more about this vulnerability, visit https://www.kb.cert.org/vuls/id/906424

# Vulnerability Info

Overview

Microsoft Windows task scheduler contains a local privilege escalation vulnerability in the Advanced Local Procedure Call (ALPC) interface, which can allow a local user to obtain SYSTEM privileges.


Description

The Microsoft Windows task scheduler SchRpcSetSecurity API contains a vulnerability in the handling of ALPC, which can allow a local user to gain SYSTEM privileges. We have confirmed that the public exploit code works on 64-bit Windows 10 and Windows Server 2016 systems. Compatibility with other Windows versions may be possible with modification of the publicly-available exploit source code.


Impact

A local user may be able to gain elevated (SYSTEM) privileges.


Solution

The CERT/CC is currently unaware of a practical solution to this problem.
