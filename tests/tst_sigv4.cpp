#include "jot/SigV4.h"

#include <QTest>

using namespace Qt::StringLiterals;

class TestSigV4 : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void signsAsAwsDocumentsIt();
    void encodesAsAwsWants();
};

void TestSigV4::signsAsAwsDocumentsIt()
{
    const QByteArray empty = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    // AWS's worked example of signing an IAM request.
    const QDateTime iamWhen(QDate(2015, 8, 30), QTime(12, 36), QTimeZone::UTC);
    jot::SigV4Request iam{"GET",
                          "/",
                          {{u"Action"_s, u"ListUsers"_s}, {u"Version"_s, u"2010-05-08"_s}},
                          {{"content-type", "application/x-www-form-urlencoded; charset=utf-8"},
                           {"host", "iam.amazonaws.com"},
                           {"x-amz-date", jot::amzDate(iamWhen)}},
                          empty};
    QCOMPARE(
        jot::sigV4Authorization(iam, u"AKIDEXAMPLE"_s,
                                u"wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY"_s, u"us-east-1"_s,
                                u"iam"_s, iamWhen),
        QByteArray("AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/iam/"
                   "aws4_request, SignedHeaders=content-type;host;x-amz-date, "
                   "Signature=5d672d79c15b13162d9279b0855cfba6789a8edb4c82c400e06b5924a6f2b5d7"));

    // And of getting part of an S3 object.
    const QDateTime s3When(QDate(2013, 5, 24), QTime(0, 0), QTimeZone::UTC);
    jot::SigV4Request get{"GET",
                          "/test.txt",
                          {},
                          {{"host", "examplebucket.s3.amazonaws.com"},
                           {"range", "bytes=0-9"},
                           {"x-amz-content-sha256", empty},
                           {"x-amz-date", jot::amzDate(s3When)}},
                          empty};
    QCOMPARE(
        jot::sigV4Authorization(get, u"AKIAIOSFODNN7EXAMPLE"_s,
                                u"wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"_s, u"us-east-1"_s,
                                u"s3"_s, s3When),
        QByteArray("AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/"
                   "aws4_request, SignedHeaders=host;range;x-amz-content-sha256;x-amz-date, "
                   "Signature=f0e8bdb87c964420e857bd35b5d6ed310bd44f0170aba48dd91039c6036bdb41"));
}

void TestSigV4::encodesAsAwsWants()
{
    QCOMPARE(jot::awsEncode(u"Ashfall/Mara Vey.md"_s, true), QByteArray("Ashfall/Mara%20Vey.md"));
    QCOMPARE(jot::awsEncode(u"a/b+c=d~e"_s), QByteArray("a%2Fb%2Bc%3Dd~e"));
    QCOMPARE(jot::awsEncode(u"é"_s), QByteArray("%C3%A9"));
}

QTEST_GUILESS_MAIN(TestSigV4)
#include "tst_sigv4.moc"
